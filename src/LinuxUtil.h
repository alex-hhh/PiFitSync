#pragma once

#include "Tools.h"

#include <vector>
#include <string>
#include <exception>

namespace FitSync 
{
    /** Convenience class to throw exception with errno error codes and get
     * proper error names for them. */
    class UnixException : public std::exception
    {
    public:
        UnixException(const char *who, int error_code);
        UnixException (const std::string &who, int error_code);
        const char* what() const noexcept(true);
        int error_code() { return m_ErrorCode; }

    private:
        std::string m_Who;
        int m_ErrorCode;
        mutable std::string m_Message;
        mutable bool m_MessageDone;
    };

    /** Read the contents of file_name into data.  An exception is thrown if
     * there is a problem, in which case the contents of 'data' is
     * undefined.
     */
    void ReadData(const std::string &file_name, Buffer &data);

    /** Write the contents of 'data' to 'file_name'.  This is done such that
     * there is a minimal chance of having partial data written to disk.
     */
    void WriteData(const std::string &file_name, const Buffer &data);

    /** Make sure that all directories in 'path' exist (create them if they
     * don't)
     */
    void MakeDirectoryPath(const std::string &path);

    void RemoveFile(const std::string &file_name);

    /** Return the base path where user data is to be stored.  On Linux, this
     * is the user's home directory.
     */
    std::string GetUserDataDir();

    /** Try to write the current process PID into `pid_file_name' in an
     * exclusive mode, will fail if another running process has its PID
     * written in the same file. Return true if the PID lock was succesfully
     * aquired.
     */
    bool AquirePidLock(const char *pid_file_name);

    /** Release the PID lock aquired by `AquirePidLock()'. */
    void ReleasePidLock(const char *pid_file_name);

};                                      // end namespace FitSync
