/*	EmbeddedFiles.cpp
	Copyright (C) Tom Mulgrew, 2004 (tmulgrew@slingshot.co.nz)

	Mechanism for embedding files inside executables.
*/

#include "EmbeddedFiles.h"
#include <assert.h>
#include <string.h>
#include "../FunctionLibs/JonWindows.h"
#include "../VM/vmTypes.h"

///////////////////////////////////////////////////////////////////////////////
// EmbeddedFile

EmbeddedFile::EmbeddedFile (char *rawData) {
	int offset = 0;
	EmbeddedFile (rawData, offset);
}
EmbeddedFile::EmbeddedFile (char *rawData, int& offset) {
	assert (rawData != NULL);

	// Read filename length
	int nameLength = *((int *) (rawData + offset));
	offset += sizeof (nameLength);

	// Read filename
	m_filename = rawData + offset;
	offset += nameLength;

	// Read length
	m_length = *((int *) (rawData + offset));
	offset += sizeof (m_length);

	// Save pointer to data
	m_data = rawData + offset;
	offset += m_length;
}
GenericIStream *EmbeddedFile::AsStream () {
	std::stringstream *result = new std::stringstream;		// Use a string stream as temp buffer
//	MessageBox(NULL, IntToString(m_length).c_str(), "DEBUG", MB_OK);
	result->write (m_data, m_length);						// Copy file data into it
	result->seekg (0, std::ios::beg);						// Reset to start
	return result;						
}

///////////////////////////////////////////////////////////////////////////////
// EmbeddedFiles
EmbeddedFiles::EmbeddedFiles (char *rawData) {
	int offset = 0;
	EmbeddedFiles (rawData, offset);
}
EmbeddedFiles::EmbeddedFiles (char *rawData, int& offset) {
	AddFiles (rawData, offset);
}
void EmbeddedFiles::AddFiles (char *rawData, int& offset) {
	assert (rawData != NULL);
	
	// Read # of files
	int count = *((int *) (rawData + offset));
	offset += sizeof (count);

	// Read in each file
	for (int i = 0; i < count; i++) {
		EmbeddedFile f (rawData, offset);
		m_files [f.Filename ()] = f;
//		MessageBox(NULL, f.Filename().c_str(), "Debug", MB_OK);
	}	
}
bool EmbeddedFiles::IsStored (std::string filename) {
	std::string processedName = ProcessPath (filename);
	return m_files.find (processedName) != m_files.end ();
}
GenericIStream *EmbeddedFiles::Open (std::string filename) {
	return IsStored (filename) 
		? m_files [ProcessPath (filename)].AsStream ()
		: NULL;
}
GenericIStream *EmbeddedFiles::OpenOrLoad (std::string filename) {

	// Try embedded files first
	GenericIStream *result = Open (filename);	
	if (result == NULL) {
		
		// Otherwise try to load from file
		std::ifstream *diskFile = new std::ifstream (filename.c_str (), std::ios::binary | std::ios::in);
		if (!diskFile->fail ())
			result = diskFile;
		else
			delete diskFile;
	}
	return result;
}

///////////////////////////////////////////////////////////////////////////////
// FileOpener


FileOpener::FileOpener () { 
	; 
}
FileOpener::FileOpener (char *rawData) : m_embeddedFiles (rawData) { 
	; 
}
void FileOpener::AddFiles (char *rawData) {
	int offset = 0;
	m_embeddedFiles.AddFiles (rawData, offset);
}
void FileOpener::AddFiles (char *rawData, int& offset) {
	m_embeddedFiles.AddFiles (rawData, offset);
}
bool FileOpener::CheckFilesFolder (std::string filename) {

	// Return true if the relative filename starts with "files\"
	// I.e the file is in the files\ subdirectory of the current directory
	if (ProcessPath (filename).substr (0, 6) == "files\\") 
		return true;
	else {
		SetError ("You can only open files in the \"files\" subdirectory.");
		return false;
	}
}
GenericIStream *FileOpener::OpenRead (std::string filename, bool filesFolder) {
	ClearError ();
	if (filesFolder && !CheckFilesFolder (filename))
		return NULL;
	else {
		GenericIStream *result = m_embeddedFiles.OpenOrLoad (filename);
		if (result == NULL)
			SetError ("Failed to open " + filename);
		return result;
	}
}
GenericOStream *FileOpener::OpenWrite (std::string filename, bool filesFolder) {
	ClearError ();
	if (filesFolder && !CheckFilesFolder (filename))
		return NULL;
	else {
		std::ofstream *file = new std::ofstream (filename.c_str (), std::ios::binary | std::ios::out);
		if (file->fail ()) {
			delete file;
			SetError ("Failed to open " + filename);
			return NULL;
		}
		else
			return file;
	}
}
std::string FileOpener::FilenameForRead (std::string filename, bool filesFolder) {
//	MessageBox(NULL, filename.c_str(), "Debug", MB_OK);
	
	ClearError ();
	if (filesFolder && !CheckFilesFolder (filename))
		return "";
    else {

        // Stored in embedded file?
        if (m_embeddedFiles.IsStored (filename)) {

//			MessageBox(NULL, "Embedded file", "Debug", MB_OK);

            // Find the OS's temporary directory
            char buffer [1024];
            //GetTempPath (1023, buffer);
            std::string path = buffer;

            // Ensure trailing backslash is present
            if (path != "" && path [path.length () - 1] != '\\')
                path += '\\';

            // Create temporary filename
            std::string tempFilename = path + "temp.dat";

//			MessageBox(NULL, tempFilename.c_str(), "Debug", MB_OK);
            
			// Find data in embedded file
            GenericIStream *data = m_embeddedFiles.Open (filename);
            if (data == NULL) {
//				MessageBox(NULL, "Error opening embedded file", "Debug", MB_OK);
                SetError ("Error opening embedded file");
                return "";
            }

            // Copy to temp file
            std::ofstream tempFile (tempFilename.c_str (), std::ios::out | std::ios::binary);
            data->seekg (0, std::ios::beg);
            CopyStream (*data, tempFile);
            delete data;
            if (tempFile.fail ()) {
//				MessageBox(NULL, "Error creating temp file", "Debug", MB_OK);
                SetError ("Error creating temp file");
                return "";
            }

            // Return new filename
            return tempFilename;
        }
        else {

//			MessageBox(NULL, "Disk file", "Debug", MB_OK);

            // Not embedded. Return input filename.
            return filename;
		}
    }
}

///////////////////////////////////////////////////////////////////////////////
// Routines

void PrepPathForComp (char *string) {

	// Prepare file path for comparison by converting to lowercase, and swapping '/' with '\'
	for (char *ptr = string; *ptr != 0; ptr++)
		if (*ptr >= 'A' && *ptr <= 'Z')
			*ptr = *ptr - 'A' + 'a';
		else if (*ptr == '/')
			*ptr = '\\';
}
std::string ProcessPath (std::string filename) {

	// Find full pathname
	char fullPath[1024];
	char *fileBit;
	//int r = GetFullPathName (filename.c_str (), 1024, fullPath, &fileBit);
	int r = 0;
	if (r == 0 || r > 1024)
		return filename;
	PrepPathForComp (fullPath);

	// Find current directory, convert to lowercase
	char currentDir[1024];
	//r = GetCurrentDirectory (1024, currentDir);
	r=  0;
	if (r == 0 || r > 1024)
		return (std::string) fullPath;
	PrepPathForComp (currentDir);
	int curDirLen = strlen (currentDir);
	if (currentDir [curDirLen - 1] != '\\') {
		currentDir [curDirLen] = '\\';
		currentDir [curDirLen + 1] = 0;
	}

	// Compare strings for matching directories
	bool relative = false, matches = true;
	int offset = 0;
	while (matches) {
		char	*p1 = strchr (fullPath + offset, '\\'),
				*p2 = strchr (currentDir + offset, '\\');
		matches =	p1 != NULL && p2 != NULL														// Found dir separators
				&&	p1 - (fullPath + offset) == p2 - (currentDir + offset)							// Same # of characters
				&&	memcmp (fullPath + offset, currentDir + offset, p1 - (fullPath + offset)) == 0;	// Directories match
		if (matches) {
			relative = true;
			offset = p1 - fullPath + 1;
		}
	}

	if (!relative)
		return (std::string) fullPath;
	else {
		std::string result = "";
		
		// Look for remaining directories in currentDir that didn't match 
		// For each of these, we need to back out one directory.
		char *cursor = currentDir + offset;
		char *p;
		while ((p = strchr (cursor, '\\')) != NULL) {
			result += "..\\";
			cursor = p + 1;
		}
		
		// Append remaining bit of full path
		result += (fullPath + offset);

		return result;
	}
}

void CopyStream (GenericIStream& src, GenericOStream& dst, int len) {

	// Copy stream to stream
	char buffer [0x4000];
	while (len > 0x4000 || len < 0 && !src.fail()) {
		src.read (buffer, 0x4000);
		dst.write (buffer, 0x4000);
		len -= 0x4000;
	}
	if (len > 0) {
		src.read (buffer, len);
		dst.write (buffer, len);
	}
}

bool EmbedFile (std::string filename, GenericOStream& stream) {
	
	// Open file
	std::ifstream file (filename.c_str (), std::ios::binary | std::ios::in);
	if (file.fail ())
		return false;

	// Convert filename to relative
	std::string relName = ProcessPath (filename);

	// Calculate lengths
	int nameLen = relName.length () + 1;		// +1 for 0 terminator
	file.seekg (0, std::ios::end);
	int fileLen = file.tellg ();
	file.seekg (0, std::ios::beg);

	// Write data to stream
	stream.write ((char *) &nameLen, sizeof (nameLen));
	stream.write (relName.c_str (), nameLen);
	stream.write ((char *) &fileLen, sizeof (fileLen));
    CopyStream (file, stream, fileLen);
	return true;
}
