#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <unordered_set>
#include <regex>
#include <tuple>
#include "logger.h"
#include "json.hpp"

using namespace std;
using namespace nlohmann;

string LOG_FILE_NAME = "/tmp/{}_logfile";
ofstream g_outfile;
json g_regexList = json::array();
vector<regex> g_expressions;
unordered_set<string> g_errReportedTags;

void createRegexList(json regexList) {
	for(int i = 0; i < regexList.size(); i++) {
		string regexString = regexList[i]["regex"];
		regex expression(regexString);
		g_expressions.push_back(expression);
	}
}

void loadRegexlist(string rcPath) {
	fstream rcfile;
	rcfile.open(rcPath, ios::in);
	if (!rcfile) {
		cerr << "No such file: " << rcPath << endl;
		return;
	}
	rcfile >> g_regexList;
	createRegexList(g_regexList);
}

tuple<bool, json> matchRegex(string msg, string pgmName) {
	bool isMatch = false;
	json structuredEvent;

	for (int i = 0; i < g_regexList.size(); i++) {
		match_results<string::const_iterator> matchResults;
		regex_search(msg, matchResults, g_expressions[i]);
		vector<string> groups;
		// first match in groups is entire msg string
		for(int j = 1; j < matchResults.size(); j++) {
			groups.push_back(matchResults.str(i));
		}
		vector<string> params = g_regexList[i]["params"];
		string tag = g_regexList[i]["tag"];
		structuredEvent["tag"] = tag;
		structuredEvent["program"] = pgmName;
		if(params.size() != groups.size() && g_errReportedTags.find(tag) == g_errReportedTags.end()) {
			g_errReportedTags.insert(tag);
			structuredEvent["parsed"] = groups;
		} else {
			for(int j = 0; j < params.size(); j++) {
				structuredEvent[params[j]] = groups[j];
			}
			isMatch = true;
			break; // found matching event regex
		}
	}
	return make_tuple(isMatch, structuredEvent);
}

void onInit(bool isDebug, string rcPath, string pgmName) {
	loadRegexlist(rcPath);
	if (isDebug) {
		auto logfile = regex_replace(LOG_FILE_NAME, regex("\\{}"), pgmName);
		g_outfile.open(LOG_FILE_NAME);
	}
}

void onMessage(string msg, string pgmName) {
	tuple<bool, json> regexMatch = matchRegex(msg, pgmName);
	bool match = get<0>(regexMatch);
	json event = get<1>(regexMatch);

	if(g_outfile) {
		g_outfile << "match: " << match << "\noriginal message: " << msg << "\nstructured event: " << event << endl;
	}

	if(match) {
		// call event api with event
	}
}

void onExit() {
	if(g_outfile) {
		g_outfile.close();
	}
}

void run(string pgmName) {
	while(true) {
		string line;
		getline(cin, line);
		if(line.empty()) {
			continue;
		}
		onMessage(line, pgmName);
		cout << "Called onMessage on: " << line << endl;
	}
}

void showUsage(string name) {
	cerr << "Usage: " << name << " <option(s)> SOURCES\n"
		<< "Options:\n"
		<< "\t-h,--help\t\tShow this help message\n"
		<< "\t-d,--debug,type=bool\t\tRun with debug log level"
		<< "\t-r,--rc-path,required,type=string\t\tPath to rc file"
		<< "\t-p,--pgmName,required,type=string\t\tSource of program generating syslog message"
		<< endl;
}

int main(int argc, char** argv) {
	json j;

	if (argc < 2) { // required param is not provided
		showUsage(argv[0]);
		return 1;
	}

	bool isDebug = false;
	string rcPath;
	string pgmName;

	for(int i = 1; i < argc; i++) {
		string arg = argv[i];
		if (arg == "-h" || arg == "--help") {
			showUsage(argv[0]);
			return 0;
		} else { // no argument for flag
			if (i + 1 >= argc) {
				cout << "Missing argument after flag" << endl;
				showUsage(argv[0]);
				return 1;
			}
		}
		if (arg == "-d" || arg == "--debug") {
			string debugFlag = argv[++i];
			if (debugFlag != "true" && debugFlag != "false") { // invalid argument for debugFlag
				showUsage(argv[0]);
				return 1;
			}
			isDebug = (debugFlag == "true") ? true : false;
		} else if (arg == "-r" || arg == "--rc-path") {
			rcPath = argv[++i];
		} else if (arg == "p" || arg == "--pgmName") {
			pgmName = argv[++i];	
		} else { // invalid flag
			showUsage(argv[0]);
			return 1;
		}
	}
	if(rcPath.empty() || pgmName.empty()) { // Missing required rc path
		showUsage(argv[0]);
		return 1;
	}
	onInit(isDebug, rcPath, pgmName);
	run(pgmName);

	return 0;
}
