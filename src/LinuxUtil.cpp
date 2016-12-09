#include "LinuxUtil.h"

#include <algorithm>
#include <sstream>
#include <iostream>
#include <stdexcept>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pwd.h>
#include <syslog.h>
#include <signal.h>

namespace FitSync 
{

    UnixException::UnixException(const char *who, int error_code)
        : m_Who(who), m_ErrorCode(error_code), m_MessageDone(false)
    {
        // empty
    }

    const char* UnixException::what() const noexcept(true)
    {
        if (!m_MessageDone) {
            std::ostringstream msg;
            msg << m_Who << ": (" << m_ErrorCode << ") " << strerror(m_ErrorCode);
            m_Message = msg.str();
            m_MessageDone = true;
        }
        return m_Message.c_str();
    }
  
    void ReadData(const std::string &file_name, Buffer &data)
    {
        // Read data in 10kb chunks.  Most FIT files should have less than one
        // chunk.
        const int chunk_size = 10 * 1024;
        // Limit file sizes, since we are on an embedded system (Raspberry
        // PI).  2 Mb FIT files would be very large, so we should be safe with
        // this limit
        const int max_size = 2 * 1024 * 1024;

        data.clear();
        int fd = ::open(file_name.c_str(), O_RDONLY);
        if (fd == -1) {
            throw UnixException("ReadData: open", errno);
        }
        for (;;) {
            unsigned mark = data.size();
            data.resize(data.size() + chunk_size);
            int n = ::read(fd, &data[mark], chunk_size);
            if (n < 0) {
                ::close(fd);
                throw UnixException("ReadData: read", errno);
            }
            else if (n == 0) { // End of file
                data.resize(mark);
                break;
            }
            else {
                data.resize(mark + n);
            }
            if (data.size() > max_size) {
                ::close(fd);
                throw std::runtime_error("ReadData: file too big");
            }
        }
        ::close(fd);
    }

    void WriteData(const std::string &file_name, const Buffer &data)
    {
        std::ostringstream tmp_file;
        tmp_file << file_name << ".tmp";
        int fd = ::open(tmp_file.str().c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0644);
        if (fd == -1) {
            throw UnixException("WriteData: open", errno);
        }
        int n = ::write(fd, &data[0], data.size());
        if (n == -1) {
            ::close(fd);
            throw UnixException("WriteData: write", errno);
        }
        else if (n < static_cast<int>(data.size())) {
            ::close(fd);
            throw std::runtime_error("WriteData: short write");
        }
        ::close(fd);

        int r = ::rename(tmp_file.str().c_str(), file_name.c_str());

        if (r == -1) {
            throw UnixException("WriteData: rename", errno);
        }
    }

    void MakeDirectoryPath(const std::string &path)
    {
        std::vector<char> buf;
        buf.reserve(path.size() + 1);
        std::copy(path.begin(), path.end(), std::back_inserter(buf));
        buf.push_back('\0');
    
        char *tmp = &buf[0];
        for (char *p = strchr(tmp, '/'); p; p = strchr(p + 1, '/')) 
        {
            *p = '\0';
            ::mkdir(tmp, 0700);
            *p = '/';
        }
        ::mkdir(tmp, 0700);
    }

    void RemoveFile(const std::string &file)
    {
        ::unlink(file.c_str());
    }

    std::string GetUserDataDir()
    {
        const char *home = getenv("HOME");
        if (! home) {
            struct passwd *pw = getpwuid(getuid());
            home = pw->pw_dir;
        }
        if (! home) {
            throw std::runtime_error("Cannot find suitable home directory");
        }
        return std::string(home);
    }

    bool AquirePidLock(const char *pid_file_name)
    {
        char buf[32];
    
        // NOTE: the flag combination below will ensure that open() succedes
        // only if g_PidFile does not exist (and the file is created).
        int fd = open(pid_file_name, O_WRONLY | O_CREAT | O_EXCL, 0444);
        if (fd != -1)
        {
            int n = snprintf(buf, sizeof(buf), "%d", getpid());
            if (n > (int)sizeof(buf) || n < 0)
                throw std::runtime_error("AquirePidLock: snprintf failed");
            int r = write(fd, buf, strlen(buf));
            if (r == -1)
                throw UnixException("AquirePidLock: write", errno);
            close(fd);
            return true;
        }
        else
        {
            // There is a PID file, check if the PID still exists.
            int fd = open(pid_file_name, O_RDONLY, 0);
            if (fd == -1)
                throw UnixException("AquirePidLock: open", errno);
            int n = read(fd, buf, sizeof(buf) - 1);
            int e = errno;          // in case we need it
            close(fd);
            if (n == -1)
                throw UnixException("AquirePidLock: read", e);
            buf[n] = '\0';

            int other_pid = 0;
            sscanf(buf, "%d", &other_pid);
            int r = kill((pid_t)other_pid, 0);
            if (r == 0) {
                syslog(LOG_ERR, "another process is running as PID %d", other_pid);
                std::cerr << "another process is running as PID " << other_pid << "\n";
                return false;
            }

            // Remove the stale PID file and try again
            unlink(pid_file_name);
            return AquirePidLock(pid_file_name);
        }
    }

    void ReleasePidLock(const char *pid_file_name)
    {
        unlink(pid_file_name);
    }

};                                      // end namespace FitSync
