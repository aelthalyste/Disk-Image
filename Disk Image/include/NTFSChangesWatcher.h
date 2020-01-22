#pragma once

#include <windows.h>
#include <memory>

class NTFSChangesWatcher
{
public:
	NTFSChangesWatcher(char drive_letter);
	~NTFSChangesWatcher() = default;

	// Method which runs an infinite loop and waits for new update sequence number in a journal.
	// The thread is blocked till the new USN record created in the journal.
	void WatchChanges();

private:
	HANDLE OpenVolume(char drive_letter);

	bool CreateJournal();

	bool LoadJournal();

	bool WaitForNextUsn(PREAD_USN_JOURNAL_DATA read_journal_data) const;

	std::unique_ptr<READ_USN_JOURNAL_DATA> GetWaitForNextUsnQuery(USN start_usn);

	bool ReadJournalRecords(PREAD_USN_JOURNAL_DATA journal_query, LPVOID buffer,
		DWORD& byte_count) const;

	std::unique_ptr<READ_USN_JOURNAL_DATA> GetReadJournalQuery(USN low_usn);


	char mDriveLetter;

	HANDLE mVolumeHandle;

	std::unique_ptr<USN_JOURNAL_DATA> mJournalData;

	DWORDLONG mJournalID;

	USN mLastUsn;

	// Flags, which indicate which types of changes you want to listen.
	static const int FILE_CHANGE_BITMASK;

	static const int kBufferSize;
};