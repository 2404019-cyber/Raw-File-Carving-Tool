#include<iostream>
#include<vector>
#include<string>
#include<cstdio>
#include<fstream>
#include<cstring>
#include<windows.h>

using namespace std;

// ============================================================================
// FILE TYPE DEFINITIONS (Magic Numbers / Signatures)
// ============================================================================
class FileType
{
public:
    virtual string getName() const = 0;
    virtual string getExtension() const = 0;
    virtual const vector<unsigned char>& getMagicBytes() const = 0;
    virtual size_t getMagicOffset() const { return 0; } // Default offset is 0
    virtual ~FileType() {}
};

class JPEGType : public FileType
{
public:
    string getName() const override { return "JPEG"; }
    string getExtension() const override { return ".jpg"; }
    const vector<unsigned char>& getMagicBytes() const override 
    {
        static const vector<unsigned char> magic = { 0xFF, 0xD8, 0xFF };
        return magic;
    }
};

class PNGType : public FileType
{
public:
    string getName() const override { return "PNG"; }
    string getExtension() const override { return ".png"; }
    const vector<unsigned char>& getMagicBytes() const override 
    {
        static const vector<unsigned char> magic = { 0x89, 0x50, 0x4E, 0x47 };
        return magic;
    }
};

class PDFType : public FileType
{
public:
    string getName() const override { return "PDF"; }
    string getExtension() const override { return ".pdf"; }
    const vector<unsigned char>& getMagicBytes() const override 
    {
        static const vector<unsigned char> magic = { 0x25, 0x50, 0x44, 0x46 }; // %PDF
        return magic;
    }
};

class MP4Type : public FileType
{
public:
    string getName() const override { return "MP4"; }
    string getExtension() const override { return ".mp4"; }
    const vector<unsigned char>& getMagicBytes() const override 
    {
        static const vector<unsigned char> magic = { 0x66, 0x74, 0x79, 0x70 }; // ftyp chunk
        return magic;
    }
    size_t getMagicOffset() const override { return 4; } // MP4 starts signature at byte 4
};

class ZIPType : public FileType
{
public:
    string getName() const override { return "ZIP"; }
    string getExtension() const override { return ".zip"; }
    const vector<unsigned char>& getMagicBytes() const override 
    {
        static const vector<unsigned char> magic = { 0x50, 0x4B, 0x03, 0x04 }; // PK..
        return magic;
    }
};

class EXEType : public FileType
{
public:
    string getName() const override { return "EXE"; }
    string getExtension() const override { return ".exe"; }
    const vector<unsigned char>& getMagicBytes() const override 
    {
        static const vector<unsigned char> magic = { 0x4D, 0x5A }; // MZ
        return magic;
    }
};


// ============================================================================
// RAW SECTOR CARVING ENGINE
// ============================================================================
class RecoveryEngine
{
private:
    string getExecutableDirectory()
    {
        char buffer[MAX_PATH];
        GetModuleFileNameA(NULL, buffer, MAX_PATH);
        string exepath(buffer);
        size_t pos = exepath.find_last_of("\\/");
        return (pos == string::npos) ? "" : exepath.substr(0, pos);
    }

public:
    void carveDrive(const string& driveLetter, const FileType& type)
    {
        // Construct the raw device access path using standard string (e.g., \\.\H:)
        string rawPath = "\\\\.\\" + driveLetter;
        if (!rawPath.empty() && rawPath.back() == '\\') 
        {
            rawPath.pop_back(); 
        }

        HANDLE hDrive = CreateFileA(
            rawPath.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );

        if (hDrive == INVALID_HANDLE_VALUE) {
            cerr << "[ERROR] Failed to open raw drive. Run as Administrator! Error code: " << GetLastError() << endl;
            return;
        }

        // Setup paths based entirely on narrow strings to comply with MinGW ofstream
        string exeDir = getExecutableDirectory();
        string baseDir = exeDir + "\\Recovered";
        string subDir = baseDir + "\\" + type.getName();

        // Create the folder architecture layout locally
        CreateDirectoryA(baseDir.c_str(), NULL);
        CreateDirectoryA(subDir.c_str(), NULL);

        cout << "\n[START] Deep-Carving raw disk sectors from: " << rawPath << endl;
        cout << "[TARGET] Saving output straight to: " << subDir << endl;

        const DWORD BUFFER_SIZE = 1024 * 1024; // 1 MB buffer chunking
        vector<unsigned char> buffer(BUFFER_SIZE);
        DWORD bytesRead = 0;
        int fileCount = 0;

        const auto& magic = type.getMagicBytes();
        size_t sigSize = magic.size();
        size_t sigOffset = type.getMagicOffset();

        // Direct Stream Reader Loop
        while (ReadFile(hDrive, &buffer[0], BUFFER_SIZE, &bytesRead, NULL) && bytesRead > 0)
        {
            if (bytesRead < (sigSize + sigOffset))
                continue;

            // Search through the raw buffer window
            for (DWORD i = 0; i <= bytesRead - (sigSize + sigOffset); ++i)
            {
                bool match = true;
                for (size_t j = 0; j < sigSize; ++j)
                {
                    if (buffer[i + sigOffset + j] != magic[j])
                    {
                        match = false;
                        break;
                    }
                }

                if (match)
                {
                    fileCount++;
                    char numberBuffer[32];
                    sprintf(numberBuffer, "%d", fileCount);

                    // Output file designation string (MinGW completely supports string here)
                    string outPath = subDir + "\\carved_" + type.getName() + "_" + numberBuffer + type.getExtension();
                    
                    ofstream outFile(outPath, ios::binary);
                    if (outFile.is_open())
                    {
                        // Write out the raw data block starting from match point 'i'
                        outFile.write(reinterpret_cast<const char*>(&buffer[i]), bytesRead - i);
                        outFile.close();
                        cout << "[FOUND] Carved " << type.getName() << " sequence -> " << outPath << endl;
                    }
                }
            }
        }

        CloseHandle(hDrive);
        cout << "\n=========================================\n";
        cout << " Raw Carving Operation Complete.\n";
        cout << " Total " << type.getName() << " Files Recovered: " << fileCount << "\n";
        cout << "=========================================\n";
    }
};


// ============================================================================
// SYSTEM MANAGER
// ============================================================================
class RecoveryManager
{
private:
    vector<FileType*> fileTypes;

public:
    RecoveryManager()
    {
        fileTypes.push_back(new JPEGType());
        fileTypes.push_back(new PNGType());
        fileTypes.push_back(new PDFType());
        fileTypes.push_back(new MP4Type());
        fileTypes.push_back(new ZIPType());
        fileTypes.push_back(new EXEType());
    }

    ~RecoveryManager()
    {
        for (auto type : fileTypes) 
            delete type;
    }

    vector<string> getLogicalDrivesList()
    {
        vector<string> drives;
        DWORD mask = GetLogicalDrives();

        for (char letter = 'A'; letter <= 'Z'; letter++)
        {
            if (mask & (1 << (letter - 'A')))
            {
                string drive = "";
                drive += letter;
                drive += ":";
                drives.push_back(drive);
            }
        }
        return drives;
    }

    void run()
    {
        RecoveryEngine engine;

        while (true)
        {
            vector<string> drives = getLogicalDrivesList();
            cout << "\n===== AVAILABLE TARGET DRIVES =====\n";
            for (size_t i = 0; i < drives.size(); i++)
            {
                cout << i + 1 << ". " << drives[i] << endl;
            }
            cout << "0. Exit Application\n";

            int driveChoice;
            cout << "\nSelect drive number to scan: ";
            cin >> driveChoice;

            if (driveChoice == 0) break;
            if (driveChoice < 1 || driveChoice > (int)drives.size())
            {
                cout << "Invalid drive configuration.\n";
                continue;
            }

            string selectedDrive = drives[driveChoice - 1];

            cout << "\n===== RUN CARVING BY SIGNATURE =====\n";
            for (size_t i = 0; i < fileTypes.size(); i++)
            {
                cout << i + 1 << ". " << fileTypes[i]->getName() << endl;
            }

            int fileChoice;
            cout << "\nSelect Signature Target: ";
            cin >> fileChoice;

            if (fileChoice < 1 || fileChoice > (int)fileTypes.size())
            {
                cout << "Invalid signature assignment selection.\n";
                continue;
            }

            engine.carveDrive(selectedDrive, *fileTypes[fileChoice - 1]);
        }
    }
};

int main()
{
    RecoveryManager manager;
    manager.run();

    system("pause");
    return 0;
}
