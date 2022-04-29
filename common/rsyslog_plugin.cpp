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

string parserCfgFile = "{}_parse.rc.json";
ofstream outfile;
string logfile = "/tmp/{}_logfile";
json regexList = json::array();
string pgmName = "";
unordered_set<string> errReportedTags;

void loadRegexlist(string rcPath, string pgmName) {
	fstream rcfile;
	parserCfgFile = regex_replace(parserCfgFile, regex("\\{}"), pgmName);
	parserCfgFile = rcPath + parserCfgFile;
	rcfile.open(parserCfgFile, ios::in);
	if (!rcfile) {
		cerr << "No such file: " << parserCfgFile << endl;
		return;
	}
	rcfile >> regexList;
}

tuple<bool, json> matchRegex(string msg) {
	bool isMatch = false;
	json structuredEvent;
	for (int i = 0; i < regexList.size(); i++) {
		string regexString = regexList[i]["regex"];
		regex expression(regexString);
		match_results<string::const_iterator> matchResults;
		regex_search(msg, matchResults, expression);
		vector<string> groups;
		// first match in groups is entire msg string
		for(int j = 1; j < matchResults.size(); j++) {
			groups.push_back(matchResults.str(i));
		}
		vector<string> params = regexList[i]["params"];
		string tag = regexList[i]["tag"];
		structuredEvent["tag"] = tag;
		structuredEvent["program"] = pgmName;
		if(params.size() != groups.size() && errReportedTags.find(tag) == errReportedTags.end()) {
			errReportedTags.insert(tag);
			structuredEvent["parsed"] = groups;
		} else {
			for(int j = 0; j < params.size(); j++) {
				structuredEvent[params[j]] = "groups[j]";
			}
			isMatch = true;
			break;
		}
	}
	return make_tuple(isMatch, structuredEvent);
}

void onInit(bool isDebug, string rcPath, string pgmName) {
	loadRegexlist(rcPath, pgmName);
	if (isDebug) {
		logfile = regex_replace(logfile, regex("\\{}"), pgmName);
		outfile.open(logfile);
	}
}

void onMessage(string msg) {
	tuple<bool, json> regexMatch = matchRegex(msg);
	bool match = get<0>(regexMatch);
	json event = get<1>(regexMatch);

	if(outfile) {
		outfile << "match: " << match << "\noriginal message: " << msg << "\nstructured event: " << event << endl;
	}

	if(match) {
		// call event api with event
	}
}

void onExit() {
	if(outfile) {
		outfile.close();
	}
}

void run() {
	while(true) {
		string line;
		getline(cin, line);
		if(line.empty()) {
			continue;
		}
		onMessage(line);
		cout << "Called onMessage on: " << line << endl;
	}
}

void showUsage(string name) {
	cerr << "Usage: " << name << " <option(s)> SOURCES\n"
		<< "Options:\n"
		<< "\t-h,--help\t\tShow this help message\n"
		<< "\t-d,--debug,type=bool\t\tRun with debug log level"
		<< "\t-p,--pgm-name,type=string,required=True\t\tName of the program dumping the message"
		<< "\t-r,--rc-path\t\tDirectory of the rc file"
		<< endl;
}

int main(int argc, char** argv) {
	json j;

	if (argc < 2) { // required param is not provided
		showUsage(argv[0]);
		return 1;
	}

	bool isDebug = false;
	string programName = "";
	string rcPath = "/etc/rsyslog.d/";

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
		} else if (arg == "-p" || arg == "--pgm-name") {
			programName = argv[++i];
		} else if (arg == "-r" || arg == "--rc-path") {
			rcPath = argv[++i];
		} else { // invalid flag
			showUsage(argv[0]);
			return 1;
		}
	}
	pgmName = programName;
	onInit(isDebug, rcPath, programName);
	run();

	return 0;
}
