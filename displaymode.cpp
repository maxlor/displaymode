#include <cassert>
#include <iomanip>
#include <iostream>
#include <getopt.h>
#include <map>
#include <regex>
#include <unordered_set>
#include <unistd.h>
extern "C" {
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
}

using namespace std;
enum LONG_OPTS { LIST_OUTPUTS = 1000, LIST_MODES = 1001 };
static const string CSI = "\x1b["; // ANSI escape code
static const string BOLD = CSI + "1m";
static const string UNDERLINE = CSI + "4m";
static const string REVERSE = CSI + "7m";
static const string RESET = CSI + "0m";

struct OutputModes {
	RRMode currentMode;
	int currentWidth;
	int currentHeight;
	unordered_set<RRMode> preferredModes;
	map<int, map<int, multimap<double, RRMode>>> availableModes; // keys: width, height, rate
};

bool gatherData();
void listRates(const string &output);
void listModes(const string &output);
void listOutputs();
double refreshRate(const XRRModeInfo *modeInfo);
OutputModes getOutputModes(const string &output);
RRMode findMode(const string &output, const string &wantedMode);
bool setMode(const string &output, const string &wantedMode);
void showUsage(string name);


map<RRMode, const XRRModeInfo*> modes;
map<const string, const XRROutputInfo*> outputs;
map<const string, RRMode> currentModes;
map<const string, RRCrtc> crtcs;
string primaryOutput;
Display *display = nullptr;
XRRScreenResources *screenRes = nullptr;
RRCrtc crtc = 0;


int main(int argc, char *argv[])  {
	bool flagHelp = false;
	bool flagListOutputs = false;
	bool flagListModes = false;
	bool flagListRates = false;
	bool flagSetMode = true;
	string output;
	string wantedMode;
	
	for (;;) {
		int index = 0;
		static struct option LongOptions[] = {
		{ "help", no_argument, nullptr, 'h' },
		{ "list-outputs", no_argument, nullptr, LIST_OUTPUTS },
		{ "list-modes", no_argument, nullptr, LIST_MODES },
		{ "output", required_argument, 0, 'o' },
		};
		
		int c = getopt_long(argc, argv, "ho:", LongOptions, &index);
		
		if (c == -1) { break; }
		
		switch (c) {
		case 'h': flagHelp = true; break;
		case 'o': output = optarg; break;
		case LIST_OUTPUTS: flagListOutputs = true; flagSetMode = false; break;
		case LIST_MODES: flagListModes = true; flagSetMode = false; break;
		}
	}
	
	if (optind < argc) {
		wantedMode = argv[optind];
	} else {
		flagSetMode = false;
	}
	
	if (flagHelp) { showUsage(argv[0]); return 0; }
	
	if (not (flagListModes or flagListOutputs or flagSetMode)) {
		flagListRates = true;
	}
	
	if (not gatherData()) { return -1; }
	if (output.empty()) {
		output = primaryOutput;
		if (output.empty()) {
			cerr << "Error: cannot determine primary output" << endl;
			return -2;
		}
	} else if (outputs.find(output) == outputs.end()) {
		cerr << "Error: no output named \"" << output << "\"" << endl;
		return -3;
	}
	
	if (flagListRates) { listRates(output); }
	if (flagListModes) { listModes(output); }
	if (flagListOutputs) { listOutputs(); }
	if (flagSetMode) { return setMode(output, wantedMode) ? 0 : -4; }
	
	return 0;
}


bool gatherData() {
	display = XOpenDisplay(nullptr);
	if (display == nullptr) {
		cerr << "Error: can't open display " << XDisplayName(nullptr) << endl;
		return false;
	}
	
	int screen = DefaultScreen(display);
	if (screen < 0 or screen >= ScreenCount(display)) {
		cerr << "Error: can't open default screen" << endl;
		return false;
	}
	
	Window window = RootWindow(display, screen);
	screenRes = XRRGetScreenResourcesCurrent(display, window);
	
	for (int i = 0; i < screenRes->nmode; ++i) {
		const XRRModeInfo *modeInfo = screenRes->modes + i;
		modes.insert({ modeInfo->id, modeInfo });
	}
	
	RROutput primary = XRRGetOutputPrimary(display, window);
	
	for (int i = 0; i < screenRes->noutput; ++i) {
		const XRROutputInfo *outputInfo = XRRGetOutputInfo(display, screenRes, screenRes->outputs[i]);
		outputs.insert({ outputInfo->name, outputInfo });
		
		if (screenRes->outputs[i] == primary) {
			primaryOutput = outputInfo->name;
		}
		
		crtcs.insert({ outputInfo->name, outputInfo->crtc });
		if (outputInfo->crtc) {
			const XRRCrtcInfo *crtcInfo = XRRGetCrtcInfo(display, screenRes, outputInfo->crtc);
			currentModes.insert({ outputInfo->name, crtcInfo->mode });
		}
	}
	
	return true;
}


void listRates(const string &output) {
	OutputModes outputModes = getOutputModes(output);
	const RRMode &currentMode = outputModes.currentMode;
	const unordered_set<RRMode> &preferredModes = outputModes.preferredModes;;
	const map<int, map<int, multimap<double, RRMode>>> &modeMap = outputModes.availableModes;
	
	cout << "Refresh rates for " << output << ":" << endl;
	if (modes.size() == 0) {
		cout << "  (no refresh rates)" << endl;
		return;
	}
	
	const multimap<double, RRMode> &rateMap = modeMap.at(outputModes.currentWidth).at(outputModes.currentHeight);
	
	string wxh = to_string(outputModes.currentWidth) + "x" + to_string(outputModes.currentHeight) + "@...";
	cout << "  " << left << setw(15) << wxh;
	
	for (auto it = rateMap.crbegin(); it != rateMap.crend(); ++it) {
		double rate = it->first;
		RRMode mode = it->second;
		
		string attrStart = "";
		string attrEnd = "";
		
		if (currentMode == mode)            { attrStart += REVERSE; }
		if (preferredModes.count(mode) > 0) { attrStart += BOLD; }
		if (not attrStart.empty())          { attrEnd += RESET; }
		
		char buffer[8];
		snprintf(buffer, sizeof(buffer), "%.2f", rate);
		string rateString = buffer;
		if (rateString.substr(rateString.size() - 3, 3) == ".00") {
			rateString = rateString.substr(0, rateString.size() - 3);
		}
		string padString(6 - rateString.size(), ' ');
		
		cout << "  " << attrStart << rateString << attrEnd << padString;
	}
	
	cout << endl;
}


void listModes(const string &output) {
	cout << "Modes for " << output << ":" << endl;
	if (modes.size() == 0) {
		cout << "  (no modes)" << endl;
		return;
	}
	
	OutputModes outputModes = getOutputModes(output);
	const RRMode &currentMode = outputModes.currentMode;
	const unordered_set<RRMode> &preferredModes = outputModes.preferredModes;;
	const map<int, map<int, multimap<double, RRMode>>> &modeMap = outputModes.availableModes;
	
	for (auto it = modeMap.crbegin(); it != modeMap.crend(); ++it) {
		string width = to_string(it->first);
		auto widthMap = it->second;
		for (auto it = widthMap.crbegin(); it != widthMap.crend(); ++it) {
			string height = to_string(it->first);
			string wxh = width + "x" + height + "@...";
			cout << "  " << left << setw(15) << wxh;
			
			auto rateMap = it->second;
			for (auto it = rateMap.crbegin(); it != rateMap.crend(); ++it) {
				double rate = it->first;
				RRMode mode = it->second;
				
				string attrStart = "";
				string attrEnd = "";
				
				if (currentMode == mode)            { attrStart += REVERSE; }
				if (preferredModes.count(mode) > 0) { attrStart += BOLD; }
				if (not attrStart.empty())          { attrEnd += RESET; }
				
				char buffer[8];
				snprintf(buffer, sizeof(buffer), "%.2f", rate);
				string rateString = buffer;
				if (rateString.substr(rateString.size() - 3, 3) == ".00") {
					rateString = rateString.substr(0, rateString.size() - 3);
				}
				string padString(6 - rateString.size(), ' ');
				
				cout << "  " << attrStart << rateString << attrEnd << padString;
			}
			
			cout << endl;
		}
	}
}


void listOutputs() {
	cout << "Outputs:" << endl;
	if (outputs.size() > 0) {
		for (auto it = outputs.cbegin(); it != outputs.cend(); ++it) {
			string attrStart = "";
			string attrEnd = "";
			
			if (it->first == primaryOutput) {
				attrStart = BOLD;
				attrEnd = RESET;
			}
			
			cout << "  " << attrStart << it->first << attrEnd << endl;
		}
	} else {
		cout << "  (no outputs)" << endl;
	}
}


double refreshRate(const XRRModeInfo *modeInfo) {
	int pixels = modeInfo->hTotal * modeInfo->vTotal;
	
	if (pixels > 0) {
		if (modeInfo->modeFlags & RR_Interlace) {
			pixels >>= 1;
		}
		if (modeInfo->modeFlags & RR_DoubleScan) {
			pixels <<= 1;
		}
		
		return modeInfo->dotClock / (double)pixels;
	}
	
	return 0.0;
}


OutputModes getOutputModes(const string &output) {
	OutputModes result;
	result.currentMode = -1;
	result.currentWidth = -1;
	result.currentHeight = -1;
	
	// insert modes into a multi-dimensional map for sorting
	const XRROutputInfo *outputInfo = outputs[output];
	
	auto it = currentModes.find(output);
	if (it != currentModes.end()) {
		result.currentMode = it->second;
	}
	
	for (int i = 0; i < outputInfo->nmode; ++i) {
		const XRRModeInfo *mode = modes[outputInfo->modes[i]];
		result.availableModes[mode->width][mode->height].insert({ refreshRate(mode), mode->id });
		if (mode->id == result.currentMode) {
			result.currentWidth = mode->width;
			result.currentHeight = mode->height;
		}
		
		if (i < outputInfo->npreferred) {
			result.preferredModes.insert(mode->id);
		}
	}
	
	return result;
}


RRMode findMode(const string &output, const string &wantedMode) {
	static const regex ModeRegex(R"((?:(\d+)x(\d+)(?:@(\d+(?:\.\d*))?)?|(\d+(?:\.\d*)?)))");
	smatch m;
	ulong width = 0;
	ulong height = 0;
	double rate = 0;
	
	// The regular expression accepts the following mode specifications:
	// WIDTHxHEIGHT, e.g. 3840x2160. The values are in sub matches 1 and 2
	// WITDHxHEIGHT@RATE, e.g. 3840x2160x60. The values are in sub matches 1, 2 and 3
	// RATE, e.g. 60. The value is in sub match 4
	if (regex_match(wantedMode, m, ModeRegex)) {
		assert(m.size() == 5);
		if (m[1].length() > 0) {
			assert(m[2].length() > 0);
			assert(m[4].length() == 0);
			
			width = stoul(m[1].str());
			height = stoul(m[2].str());
			if (m[3].length() > 0) {
				rate = stod(m[3].str());
			}
		} else {
			assert(m[1].length() == 0 and m[2].length() == 0 and m[3].length() == 0);
			assert(m[4].length() > 0);
			rate = stod(m[4].str());
		}
	} else {
		cerr << "Error: cannot parse mode \"" << wantedMode << "\"" << endl;
		return 0;
	}
	
	const XRRModeInfo *currentModeInfo = modes[currentModes[output]];
	if (width == 0) {
		width = currentModeInfo->width;
		height = currentModeInfo->height;
	} else if (rate == 0) {
		rate = refreshRate(currentModeInfo);
	}
	
	const XRROutputInfo *outputInfo = outputs[output];
	map<int, map<int, multimap<double, RRMode>>> modeMap;
	
	for (int i = 0; i < outputInfo->nmode; ++i) {
		const XRRModeInfo *mode = modes[outputInfo->modes[i]];
		modeMap[mode->width][mode->height].insert({ refreshRate(mode), mode->id });
	}
	
	if (modeMap.find(width) == modeMap.end() or
	        modeMap[width].find(height) == modeMap[width].end()) {
		cerr << "Error: invalid resolution: " << width << "x" << height << endl;
		return 0;
	}
	
	const multimap<double, RRMode> &rateMap = modeMap[width][height];
	double chosenDelta = 1e99;
	RRMode chosenMode = 0;
	for (auto it : rateMap) {
		double delta = abs(it.first - rate);
		if (delta < chosenDelta) {
			chosenDelta = delta;
			chosenMode = it.second;
		}
	}
	
	if (chosenMode == 0) {
		cerr << "Error: no appropriate mode found" << endl;
		return 0;
	}
	
	return chosenMode;
}


bool setMode(const string &output, const string &wantedMode) {
	RRMode mode = findMode(output, wantedMode);
	if (mode == 0) { return false; }
	
	auto it = crtcs.find(output);
	if (it == crtcs.end()) {
		cerr << "Error: output \"" << output << "\" is disabled" << endl;
		return false;
	}
	
	RRCrtc crtc = it->second;
	const XRRCrtcInfo *crtcInfo = XRRGetCrtcInfo(display, screenRes, crtc);
	Status status = XRRSetCrtcConfig(display, screenRes, crtc, crtcInfo->timestamp, crtcInfo->x,
	                                 crtcInfo->y, mode, crtcInfo->rotation, crtcInfo->outputs,
	                                 crtcInfo->noutput);
	
	return status == 0;
}


void showUsage(string name) {
	cout << "Usage: " << name << " [-o OUTPUT] [WIDTHxHEIGHT@]RATE" << endl;
	cout << "       " << name << " --list-outputs" << endl;
	cout << "       " << name << " --list-modes" << endl;
}
