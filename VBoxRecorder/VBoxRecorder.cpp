#include "VBoxRecorder.h"
#include "lodepng.h"
#include <algorithm>
#include <string> 
#include <ShlObj.h>
#include <fstream>

using namespace std;

enum OutputMode {
	INVALID, PNG_SEQUENCE, HUFFYUV, H265, FFV1
};

struct RecordingSettings{
	RecordingSettings() : targetPID(0), width(0), height(0), bpp(0), outputMode(HUFFYUV), deleteScreenshotAfterUse(false){}

	wstring machineID;
	DWORD targetPID;
	ULONG width, height, bpp;
	OutputMode outputMode;
	string outputFile;
	string screenshot;
	bool deleteScreenshotAfterUse;

	bool isComplete() {
		return !machineID.empty() && targetPID != 0 && width != 0 && height != 0 && bpp != 0 && !outputFile.empty();
	}
	wstring toParamString() {
		wstringstream params;
		params << "-pid " << targetPID << ' ';
		params << "-width " << width << ' ';
		params << "-height " << height << ' ';
		params << "-bpp " << bpp << ' ';
		params << "-outputmode " << outputMode << ' ';
		params << "-machine " << machineID << ' ';
		params << "-outputfile \"" << strtowstr(outputFile) << "\" ";
		params << "-screenshot \"" << strtowstr(screenshot) << "\" ";
		params << "-deletescreenshot \"" << (int)deleteScreenshotAfterUse << "\" ";
		return params.str();
	}
} settings;

bool pixelsEqual(BGRAPixel* p1, BGRAPixel* p2)
{
	return p1->blue == p2->blue &&
		   p1->green == p2->green &&
		   p1->red == p2->red;
		   //p1->alpha == p2->alpha;
}

BGRAPixel* scanPageForSequence(HANDLE procHandle, PVOID baseAddr, SIZE_T size, BGRAPixel* sequence, SIZE_T sequenceLength){
	char* data = new char[size];
	if (!ReadProcessMemory(procHandle, baseAddr, (LPVOID)data, size, &size)) {
		if (GetLastError() != ERROR_PARTIAL_COPY){
			printf("\nFailed to read process memory! (error=%d)\n", GetLastError());
			return NULL;
		}
	}

	if (size < sizeof(BGRAPixel))
	{
		return NULL;
	}
	
	//For each page until a match is found, this function is run.
	//This brute force search is O( sizeof(BGRAPixel) * page_size )
	//Better performance can be achieved by implementing Knuth-morris-pratt, booyer-moore or rabin-karp sub-string search
	for(unsigned int offset = 0; offset < sizeof(BGRAPixel); offset++)
	{
		//data will be read as BGRAPixel*
		//Make sure we don't go out of bounds
		size_t curSize = ((size - offset) / sizeof(BGRAPixel));
		BGRAPixel* pixelPtr = reinterpret_cast<BGRAPixel*>(reinterpret_cast<char*>(data)+offset);
		
		BGRAPixel* sequenceStart = NULL;
		ULONG sequenceI = 0;
		for (ULONG i = 0; i < curSize; i++) {
			if (pixelsEqual(&(pixelPtr[i]), &(sequence[sequenceI]))) {
				if (sequenceStart == NULL) {
					sequenceStart = reinterpret_cast<BGRAPixel*>(reinterpret_cast<char*>(baseAddr) + i);
				}
				sequenceI++;

				if (sequenceI == sequenceLength) {
					delete[] data;
					return sequenceStart;
				}
			}
			else {
				sequenceStart = NULL;
				sequenceI = 0;
				if (pixelsEqual(&(pixelPtr[i]), &(sequence[0]))) {
					sequenceStart = reinterpret_cast<BGRAPixel*>(reinterpret_cast<char*>(baseAddr) + i);
					sequenceI++;
				}
			}
		}

	}

	delete[] data;
	return NULL;
}

BGRAPixel* scanMemoryForSequence(HANDLE procHandle, BGRAPixel* sequence, size_t sequenceLength){
	long count = 0;
	MEMORY_BASIC_INFORMATION meminfo;
	unsigned char *addr = 0;
	while (true)
	{
		if (VirtualQueryEx(procHandle, addr, &meminfo, sizeof(meminfo)) == 0)
		{
			printf("Memory scan error %d\n", GetLastError());
			break;
		}

		printf("Scanning memory region %d [%p - %p]\r", count, meminfo.BaseAddress, (ULONG)meminfo.BaseAddress + meminfo.RegionSize);
		if ((meminfo.State & MEM_COMMIT) && (meminfo.Protect & PAGE_READWRITE))
		{
			BGRAPixel* address = scanPageForSequence(procHandle, meminfo.BaseAddress, meminfo.RegionSize, sequence, sequenceLength);
			if (address != NULL){
				printf("\n");
				return address;
			}
		}
		addr = (unsigned char*)meminfo.BaseAddress + meminfo.RegionSize;
		count++;
	}
	printf("\n");
	return NULL;
}

//Hacky solution:
// CheatEngine was used to locate the framebuffer offset in the VirtualBox process memory
// This function retrieves this offset. This breaks every time the relevant executable is updated and the offsets change
BGRAPixel* findFrameBuffer(HANDLE procHandle){
	if(!settings.screenshot.empty()){
		cout << "Scanning process memory..." << std::endl;
		ifstream inStream(settings.screenshot, ios::binary | ios::in | ios::ate);
		if (!inStream.is_open()){
			cout << "Failed to open reference screenshot" << std::endl;
			return NULL;
		}

		std::streamsize size = inStream.tellg();
		char* screenshotData = new char[size];
		inStream.seekg(0, ios::beg);
		inStream.read(screenshotData, size);
		BGRAPixel* screenshot = reinterpret_cast<BGRAPixel*>(screenshotData);
		size_t pixelCount = size / sizeof(BGRAPixel);

		BGRAPixel* frameBufferPtr = scanMemoryForSequence(procHandle, screenshot, pixelCount);
		delete[] screenshotData;
		if (frameBufferPtr != NULL){
			return frameBufferPtr;
		}
		cout << "Could not find reference screenshot in memory, attempting pointer trace." << std::endl;
	}

	LPVOID libOffset = GetRemoteLibraryAddress(procHandle, L"VBoxSharedCrOpenGL.DLL");
	if (libOffset != NULL) {
		LPCVOID frameBufferPtrAddr = (LPCVOID)((char*)libOffset + 0x001084D0);
		BGRAPixel* frameBufferPtr = NULL;
		if (!ReadProcessMemory(procHandle, frameBufferPtrAddr, (LPVOID)&frameBufferPtr, 8, NULL)) {
			cout << "Failed to read framebuffer pointer value!" << endl;
			return NULL;
		}
		return frameBufferPtr;
	}

	libOffset = GetRemoteLibraryAddress(procHandle, L"VBoxC.dll");
	if (libOffset != NULL) {
		LPCVOID addr1 = (LPCVOID)((char*)libOffset + 0x00362618);
		LPCVOID addr2 = 0;
		if (!ReadProcessMemory(procHandle, addr1, (LPVOID)&addr2, 8, NULL)) {
			return NULL;
		}
		addr2 = (LPCVOID)(((ULONG)addr2) + 0x6E8);
		BGRAPixel* frameBufferPtr = NULL;
		if (!ReadProcessMemory(procHandle, addr2, (LPVOID)&frameBufferPtr, 8, NULL)) {
			return NULL;
		}
		return frameBufferPtr;
	}

	return NULL;
}

bool recordVideo() {
	if(settings.bpp != 32) {
		cout << "Only 32bit color depth is currently supported" << endl;
		return false;
	}

	HANDLE privilegesToken;
	if(EnableDebugPrivileges(&privilegesToken) == FALSE) {
		cout << "Failed to enable debug privileges. Attempting to read anyway." << endl;
	}

	cout << "Searching framebuffer pointer in process " << settings.targetPID << endl;

	HANDLE procHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, settings.targetPID);
	if(procHandle == NULL) {
		cout << "Could not retrieve process handle! errorcode=" << GetLastError() << endl;
		return false;
	}

	BGRAPixel* frameBufferPtr = findFrameBuffer(procHandle);
	if (frameBufferPtr == NULL){
		cout << "Could not find frame buffer!" << endl;
		CloseHandle(procHandle);
		return false;
	}
	cout << "Found framebuffer at 0x" << hex << (void*)frameBufferPtr << endl;

	size_t frameBufferPixelSize = settings.width * settings.height;
	BGRAPixel* localFrameBuffer = new BGRAPixel[frameBufferPixelSize];

	//Recording setup
	int fps = 25;
	VideoEncoder encoder;
	RGBPixel* rgbBuffer = NULL;
	if(settings.outputMode == PNG_SEQUENCE) {
		rgbBuffer = new RGBPixel[frameBufferPixelSize];
		int result = SHCreateDirectory(NULL, strtowstr(settings.outputFile).c_str());
		if(result != ERROR_SUCCESS && result != ERROR_FILE_EXISTS && result != ERROR_ALREADY_EXISTS) {
			wchar_t buf[256];
			FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(),
						   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, 256, NULL);
			cout << "Could not use directory \"" << settings.outputFile << "\". (" << wstrtostr(buf) << ')' << endl;
			return false;
		}
	} else {
		EncoderSettings encoderSettings;
		encoderSettings.fps.den = fps;
		if(settings.outputMode == HUFFYUV) {
			encoderSettings.codecID = AV_CODEC_ID_HUFFYUV;
			encoderSettings.codecOptions = NULL;
			encoderSettings.format = "avi";
			encoderSettings.pixFormat = AV_PIX_FMT_RGB24;
		} else if(settings.outputMode == H265) {
			encoderSettings.codecID = AV_CODEC_ID_H265;
			AVDictionary* codecOptions = NULL;
			av_dict_set(&codecOptions, "preset", "ultrafast", 0);
			av_dict_set(&codecOptions, "tune", "zero-latency", 0);
			av_dict_set(&codecOptions, "x265-params", "lossless=true", 0);
			encoderSettings.codecOptions = codecOptions;
			encoderSettings.format = "Matroska";
			encoderSettings.pixFormat = AV_PIX_FMT_YUV444P;
			encoderSettings.strictStdCompliance = FF_COMPLIANCE_EXPERIMENTAL;
		} else if(settings.outputMode == FFV1) {
			encoderSettings.codecID = AV_CODEC_ID_FFV1;
			encoderSettings.codecOptions = NULL;
			encoderSettings.format = "Matroska";
			encoderSettings.pixFormat = AV_PIX_FMT_YUV444P;
		} else {
			cout << "Invalid outputmode!" << endl;
			return false;
		}

		encoder.setSettings(encoderSettings);
		if(!encoder.open(settings.outputFile, settings.width, settings.height)) {
			std::cout << "Failed to open video encoder" << std::endl;
			return false;
		}
	}

	//Recording loop
	cout << endl << "Press ESC to stop recording" << endl;
	DWORD time0 = GetTickCount();
	uint frameI = 0;
	bool recording = true;
	while(recording) {
		if(!ReadProcessMemory(procHandle, (LPCVOID)frameBufferPtr, localFrameBuffer, frameBufferPixelSize * sizeof(BGRAPixel), NULL)) {
			cout << "Failed to read framebuffer! errorcode=" << GetLastError() << endl;
			return false;
		}

		if(settings.outputMode == PNG_SEQUENCE) {
			for(int i = 0; i<frameBufferPixelSize; i++) {
				rgbBuffer[i] = { localFrameBuffer[i].red, localFrameBuffer[i].green, localFrameBuffer[i].blue };
			}

			string path = settings.outputFile;
			path.append("/frame").append(std::to_string(frameI)).append(".png");
			unsigned error = lodepng::encode(path, (byte*)rgbBuffer, settings.width, settings.height, LCT_RGB);
			if(error) {
				std::cout << "png encoder error " << error << ": "<< lodepng_error_text(error) << std::endl;
				return false;
			}
		} else{
			if(!encoder.recordFrame(localFrameBuffer)) {
				std::cout << "Failed to record video frame" << std::endl;
				return false;
			}
		}
		DWORD targetTime = (DWORD)(((double)(frameI+1)/(double)fps)*1000);
		PrintColoredText(FOREGROUND_LIGHT_RED, "RECORDING");
		uint hours = (targetTime / 1000) / 3600;
		uint minutes = ((targetTime / 1000) % 3600) / 60;
		uint seconds = (targetTime / 1000) % 60;
		printf(" - %02d:%02d:%02d\r", hours, minutes, seconds);

		int timediff = GetTickCount() - time0;
		int timeDelta = timediff - targetTime;
		if(timeDelta < 0) {
			Sleep(-timeDelta);
		}
		frameI++;
		if(GetAsyncKeyState(VK_ESCAPE) != 0) {
			recording = false;
		}
	}

	//Recording cleanup
	if(settings.outputMode != PNG_SEQUENCE) {
		if(!encoder.close()) {
			std::cout << "Failed to close video correctly. The video file might be corrupt." << std::endl;
			getchar();
			return false;
		}
	}
	
	cout << "Recording complete" << endl;

	CloseHandle(procHandle);
	return true;
}

bool findProcess() {
	DWORD processes[1024];
	DWORD bytesReturned = 0;
	if(!EnumProcesses(processes, sizeof(processes), &bytesReturned)) {
		cout << "Failed to enumerate processes!" << endl;
		return false;
	}
	DWORD processCount = bytesReturned / sizeof(DWORD);

	DWORD resultPID = 0;
	size_t resultPIDMemorySize = 0;
	for(uint i = 0; i<processCount; i++) {
		HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, processes[i]); //PROCESS_VM_READ for GetProcessMemoryInfo
		if(proc == NULL) {
			continue;
		}

		WCHAR path[MAX_PATH];
		if(GetProcessImageFileName(proc, path, MAX_PATH) == 0) {
			cout << "Failed to retrieve process filename! (errorcode=" << GetLastError() << ')' << endl;
			CloseHandle(proc);
			return false;
		}

		WCHAR filename[_MAX_FNAME];
		WCHAR fileext[_MAX_EXT];
		_wsplitpath_s(path, NULL, 0, NULL, 0, filename, _MAX_FNAME, fileext, _MAX_EXT);

		std::wstring name = std::wstring(filename).append(fileext);
		if(lstrcmpiW(name.c_str(), L"virtualbox.exe") == 0) {
			//This is a virtualbox process, check if its startup args contain the VM id
			//Gather all matches, choose one with largest memory usage

			bool success;
			wstring startupParams = GetProcessCommandLine(processes[i], success);
			if(!success) {
				cout << "Failed to retrieve process startup parameters!" << endl;
				CloseHandle(proc);
				return false;
			}

			if(startupParams.find(settings.machineID) != string::npos) {
				//Found a VirtualBox process with matching VM ID
				//Compare its memory usage
				PROCESS_MEMORY_COUNTERS memInfo = { sizeof(PROCESS_MEMORY_COUNTERS) };
				if(!GetProcessMemoryInfo(proc, &memInfo, memInfo.cb)) {
					cout << "Failed to retrieve process memory size!" << endl;
					CloseHandle(proc);
					return false;
				} else if(memInfo.WorkingSetSize > resultPIDMemorySize) {
					resultPID = processes[i];
					resultPIDMemorySize = memInfo.WorkingSetSize;
				}
			}
		}
		CloseHandle(proc);
	}

	if(resultPID != 0) {
		settings.targetPID = resultPID;
		return true;
	}
	return false;
}

void startRecording(IVirtualBox* vbox, ISession* session) {
	if(settings.machineID.empty()) {
		cout << "Please select a virtual machine using 'select'" << endl;
		return;
	} else if(settings.outputFile.empty()) {
		cout << "Please enter an output file using the 'outputfile' command" << endl;
		return;
	}
	cout << "Retrieving VM info" << endl;

	IMachine* machine;
	BSTR name = SysAllocStringLen(settings.machineID.data(), settings.machineID.size());
	HRESULT hr = vbox->FindMachine(name, &machine);
	SysFreeString(name);
	if(FAILED(hr)){
		cout << "Failed to retrieve machine (error=" << hex << hr << ")" << endl;
		return;
	}

	MachineState state;
	hr = machine->get_State(&state);
	if(FAILED(hr)){
		cout << "Failed to retrieve machine state (error=" << hex << hr << ")" << endl;
		machine->Release();
		return;
	}

	if(state != MachineState_Running) {
		cout << "Error: The currently selected virtual machine is not running! (state=" << state << ')' << endl;
		machine->Release();
		return;
	}

	machine->LockMachine(session, LockType::LockType_Shared);

	IConsole* console;
	hr = session->get_Console(&console);
	if(FAILED(hr)) {
		cout << "Failed to retrieve console (error=" << hex << hr << ")" << endl;
		machine->Release();
		return;
	}

	IDisplay* display;
	hr = console->get_Display(&display);
	if(FAILED(hr)) {
		cout << "Failed to retrieve display (error=" << hex << hr << ")" << endl;
		console->Release();
		machine->Release();
		return;
	}

	ULONG width, height, bpp; //bpp sometimes 0?
	LONG x, y;
	GuestMonitorStatus status;

	hr = display->GetScreenResolution(0, &width, &height, &bpp, &x, &y, &status);
	if(FAILED(hr)) {
		cout << "Failed to retrieve display information (error=" << hex << hr << ")" << endl;
		display->Release();
		console->Release();
		machine->Release();
		return;
	}

	if(bpp == 0) {
		cout << "Invalid display information. Please try again." << endl;
		display->Release();
		console->Release();
		machine->Release();
		return;
	}

	settings.width = width;
	settings.height = height;
	settings.bpp = bpp;

	stringstream screenshotFile;
	screenshotFile << rand();
	screenshotFile << ".dat";
	SAFEARRAY* screenshot;
	hr = display->TakeScreenShotToArray(0, width, height, BitmapFormat_BGRA, &screenshot);
	if (SUCCEEDED(hr)){
		BGRAPixel* pixels = NULL;
		hr = SafeArrayAccessData(screenshot, (void**)&pixels);
		if (SUCCEEDED(hr)) {
			ofstream out(screenshotFile.str(), ios::binary | ios::out);
			//out.write((char*)pixels, width * height * sizeof(BGRAPixel));
			out.write((char*)pixels, width * height/2 * sizeof(BGRAPixel));
			out.flush();
			out.close();
			settings.screenshot = screenshotFile.str();
			settings.deleteScreenshotAfterUse = true;
			SafeArrayDestroy(screenshot);
		}
		else{
			cout << "Failed to capture reference screenshot" << endl;
		}
	}
	else{
		cout << "Failed to capture reference screenshot" << endl;
	}

	display->Release();
	console->Release();
	machine->Release();

	//Instead we do this the dirty way
	//Get target process id and spawn elevated process to read framebuffer in process memory
	cout << "Retrieving VM process info" << endl;

	//Move findProcess to elevated process?
	if(!findProcess()) {
		cout << "Failed to find target process!" << endl;
		return;
	}

	if(IsElevated()) {
		recordVideo();
	} else {
		cout << "Elevating privileges to read VM process memory" << endl;
		if(!ElevateWithArgs(settings.toParamString())){
			cout << "Error: could not elevate privileges to administrator. (errorcode=" << GetLastError() << ')' << endl;
		}
	}
}

void selectMachine(IVirtualBox* vbox, vector<string>& args) {
	if(args.size() == 0) {
		cout << "Please supply a virtual machine ID, name, or list index." << endl;
		return;
	}

	wstring newMachineID;
	if(args[0].at(0) == '{') {
		newMachineID = strtowstr(args[0]); //Check id valid
	} else {
		try {
			int index = stoi(args[0]);
			newMachineID = getMachineIDFromIndex(vbox, index);
		} catch(invalid_argument) {
			stringstream name;
			for(int i = 0; i<args.size();i++) {
				name << args[i];
				if(i != args.size()-1) {
					name << ' ';
				}
			}
			newMachineID = getMachineIDFromName(vbox, name.str());
		}
	}

	if(newMachineID.empty()) {
		cout << args[0] << " is not a valid machine ID, name, or list index" << endl;
	} else {
		settings.machineID = newMachineID;
		wcout << "Machine " << settings.machineID << " was selected." << endl;
	}
}

void printMachineNames(IVirtualBox* vbox) {
	SAFEARRAY* machinesArray = NULL;
	HRESULT result;
	
	result = vbox->get_Machines(&machinesArray);
	if(SUCCEEDED(result)){
		IMachine** machines = NULL;
		result = SafeArrayAccessData(machinesArray, (void **)&machines);
		if(SUCCEEDED(result)) {
			cout << machinesArray->rgsabound[0].cElements << " virtual machines found: " << endl;
			for(ULONG i = 0; i<machinesArray->rgsabound[0].cElements; i++) {
				//Retrieve info
				BSTR name;
				result = machines[i]->get_Name(&name);
				if(FAILED(result)) {
					cout << "Failed to access machine name (error=" << hex << result << ")" << endl;
				}
				
				BSTR id;
				result = machines[i]->get_Id(&id);
				if(FAILED(result)) {
					cout << "Failed to access machine id (error=" << hex << result << ")" << endl;
				}
				
				MachineState state;
				result = machines[i]->get_State(&state);
				if(FAILED(result)) {
					cout << "Failed to access machine state (error=" << hex << result << ")" << endl;
				}

				//Print info
				wcout << " [" << i << "]";

				if(state == MachineState_Running) {
					PrintColoredText(FOREGROUND_LIGHT_GREEN, " ONLINE ");
				} else {
					PrintColoredText(FOREGROUND_RED, " OFFLINE");
				}
				
				wcout << " {name = \"" << name << "\", id=\"" << id << "\"}" << endl;

				SysFreeString(name);
				SysFreeString(id);
			}
			SafeArrayUnaccessData(machinesArray);
		}  else {
			cout << "Failed to access machine array (error=" << hex << result << ")" << endl;
		}
	} else {
		cout << "Failed to retrieve machine array (error=" << hex << result << ")" << endl;
	}
}

bool setOutputMode(string mode) {
	std::transform(mode.begin(), mode.end(), mode.begin(), tolower);
	if(mode.find("png") == 0 || mode.compare("1") == 0) {
		settings.outputMode = PNG_SEQUENCE;
	} else if(mode.find("huffyuv") == 0 || mode.compare("2") == 0) {
		settings.outputMode = HUFFYUV;
	} else if(mode.find("h265") == 0 || mode.compare("3") == 0) {
		settings.outputMode = H265;
	} else if(mode.find("ffv1") == 0 || mode.compare("4") == 0) {
		settings.outputMode = FFV1;
	} else {
		return false;
	}
	return true;
}

void setOutputMode(vector<string>& args) {
	if(args.size() == 0) {
		cout << "Available output modes:" << endl;
		SetConsoleColor(settings.outputMode == PNG_SEQUENCE ? FOREGROUND_LIGHT_GREEN : FOREGROUND_NORMAL);
		cout << " 1. PNG Sequence | lossless" << endl;
		SetConsoleColor(settings.outputMode == HUFFYUV ? FOREGROUND_LIGHT_GREEN : FOREGROUND_NORMAL);
		cout << " 2. Huffyuv avi  | lossless" << endl;
		SetConsoleColor(settings.outputMode == H265 ? FOREGROUND_LIGHT_GREEN : FOREGROUND_NORMAL);
		cout << " 3. h265 mkv     | visually lossless" << endl;
		SetConsoleColor(settings.outputMode == FFV1 ? FOREGROUND_LIGHT_GREEN : FOREGROUND_NORMAL);
		cout << " 4. FFV1 mkv     | visually lossless" << endl;
		SetConsoleColor(FOREGROUND_NORMAL);
		return;
	}

	if(setOutputMode(args[0])) {
		cout << "Output mode set to: " << args[0] << endl;
	} else {
		cout << "Unrecognized output mode \"" << args[0] << "\"" << endl;
	}
}

void setOutputFile(vector<string>& args) {
	if(args.size() == 0) {
		cout << "Please specify an output file" << endl;
		return;
	}

	settings.outputFile = args[0];
	cout << "Output file set to: " << settings.outputFile << endl;
}

void printHelp() {
	cout << "Available commands:" << endl;
	cout << " machines" << endl;
	cout << " select" << endl;
	cout << " record" << endl;
	cout << " outputmode" << endl;
	cout << " outputfile" << endl;
	cout << " help" << endl;
	cout << " exit" << endl;
}

void cmdLoop(IVirtualBoxClient* client) {
	IVirtualBox* vbox;
	if(FAILED(client->get_VirtualBox(&vbox))){
		cout << "Failed to retrieve VirtualBox API object!" << endl;
		return;
	}

	ISession* session;
	if(FAILED(client->get_Session(&session))) {
		cout << "Failed to retrieve VirtualBox session object!" << endl;
		vbox->Release();
		return;
	}

	char cmdBuf[256];
	bool running = true;

	while(running) {
		cout << '>';
		cin.getline(cmdBuf, 256);
		string cmd(cmdBuf);
		if(trim(cmd).empty()) {
			continue;
		}
		vector<string> args = split(cmd, ' ', true);
		cmd = args[0];
		args.erase(args.begin(), args.begin()+1);

		if(cmd.compare("machines") == 0) {
			printMachineNames(vbox);
		} else if(cmd.compare("select") == 0) {
			selectMachine(vbox, args);
		} else if(cmd.compare("record") == 0) {
			startRecording(vbox, session);
		} else if(cmd.compare("outputmode") == 0) {
			setOutputMode(args);
		} else if(cmd.compare("outputfile") == 0) {
			setOutputFile(args);
		} else if(cmd.compare("help") == 0) {
			printHelp();
		} else if(cmd.compare("exit") == 0) {
			running = false;
		} else {
			cout << cmd << " is not a valid command." << endl;
		}
	}

	vbox->Release();
	session->Release();
}

void parseArguments(int argc, char *argv[]) {
	for(int i = 1; i<argc; i++) { //first arg is self
		char* arg = argv[i];
		bool isLastArg = i+1 == argc;

		if(trim(string(arg)).empty()) { //Skip empty entries
			i++;
			continue;
		}

		if(strcmp(arg, "-pid") == 0 && !isLastArg) {
			try {
				settings.targetPID = stoi(string(argv[i+1]));
				HANDLE handle = OpenProcess(SYNCHRONIZE, FALSE, settings.targetPID);
				if(handle == NULL) {
					cout << "The specified PID is invalid: no process found with PID " << settings.targetPID << endl;
					exit(1);
				} else {
					CloseHandle(handle);
					i++;
				}
			} catch(invalid_argument) {
				cout << "The specified PID is invalid: PID must be an integer." << endl;
				exit(1);
			}
		} else if(strcmp(arg, "-width") == 0 && !isLastArg) {
			try {
				settings.width = stoi(string(argv[i+1]));
				i++;
			} catch(invalid_argument) {
				cout << "The specified width is invalid: width must be an integer." << endl;
				exit(1);
			}
		} else if(strcmp(arg, "-height") == 0 && !isLastArg) {
			try {
				settings.height = stoi(string(argv[i+1]));
				i++;
			} catch(invalid_argument) {
				cout << "The specified height is invalid: height must be an integer." << endl;
				exit(1);
			}
		} else if(strcmp(arg, "-bpp") == 0 && !isLastArg) {
			try {
				settings.bpp = stoi(string(argv[i+1]));
				i++;
			} catch(invalid_argument) {
				cout << "The specified bits per pixel is invalid: bpp must be an integer." << endl;
				exit(1);
			}
		} else if(strcmp(arg, "-machine") == 0 && !isLastArg) {
			settings.machineID = strtowstr(argv[i+1]);
			i++;
		} else if(strcmp(arg, "-outputmode") == 0 && !isLastArg) {
			if(!setOutputMode(argv[i+1])){
				cout << "Invalid output mode" << endl;
				exit(1);
			}
			i++;
		} else if(strcmp(arg, "-outputfile") == 0 && !isLastArg) {
			settings.outputFile = argv[i + 1];
			i++;
		}
		else if (strcmp(arg, "-screenshot") == 0 && !isLastArg) {
			settings.screenshot = argv[i + 1];
			i++;
		}
		else if (strcmp(arg, "-deletescreenshot") == 0 && !isLastArg) {
			settings.deleteScreenshotAfterUse = (bool)stoi(string(argv[i + 1]));
			i++;
		}
		else {
			cout << "Invalid argument \"" << arg << "\"" << endl;
			exit(1);
		}
	}
}

int main(int argc, char *argv[]) {
	cout << "VBoxRecorder for VirtualBox 5.1.16 r113841 Windows 64-bit" << endl;
	srand(time(NULL));
	parseArguments(argc, argv);
	if(settings.isComplete()) {
		if(!IsElevated()) {
			if(!ElevateWithArgs(settings.toParamString())) {
				cout << "Error: could not elevate privileges to administrator. (errorcode=" << GetLastError() << ')' << endl;
				return 1;
			}
			return 0;
		}

		if(!recordVideo()) {
			cout << "Press enter to continue";
			getchar();
			return 1;
		}
		return 0;
	}

	cout << "Connecting to the VirtualBox Service..." << endl;
	do {
		/* Initialize the COM subsystem. */
		CoInitializeEx(NULL, COINIT_MULTITHREADED);

		IVirtualBoxClient* client = NULL;
		HRESULT rc = CoCreateInstance(CLSID_VirtualBoxClient, NULL, CLSCTX_INPROC_SERVER, IID_IVirtualBoxClient, (void**)&client);
		if(FAILED(rc)) {
			printf("Failed to create VirtualBox client! rc = 0x%x\n", rc);
			break;
		}

		cout << "Connected and ready! Use 'help' for a list of commands." << endl;
		cmdLoop(client);

		client->Release();
	} while(0);

	if (settings.deleteScreenshotAfterUse && !settings.screenshot.empty()){
		remove(settings.screenshot.c_str());
	}

	CoUninitialize();
	return 0;
}
