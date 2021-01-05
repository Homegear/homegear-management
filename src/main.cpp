/* Copyright 2013-2019 Homegear GmbH
 *
 * Homegear is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * Homegear is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Homegear.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU Lesser General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
*/

#include "GD.h"

#include <homegear-base/Managers/ProcessManager.h>

#include <malloc.h>
#include <sys/prctl.h> //For function prctl
#ifdef BSDSYSTEM
#include <sys/sysctl.h> //For BSD systems
#endif
#include <sys/resource.h> //getrlimit, setrlimit
#include <sys/file.h> //flock
#include <sys/types.h>
#include <sys/stat.h>

#include <gcrypt.h>
#include <grp.h>
#include "../config.h"

void startUp();

GCRY_THREAD_OPTION_PTHREAD_IMPL;

bool _startAsDaemon = false;
std::thread _signalHandlerThread;
bool _stopProgram = false;
int _signalNumber = -1;
std::mutex _stopProgramMutex;
std::condition_variable _stopProgramConditionVariable;

void terminateProgram(int signalNumber) {
  try {
    GD::out.printMessage("(Shutdown) => Stopping Homegear Management (Signal: " + std::to_string(signalNumber) + ")");
    GD::bl->shuttingDown = true;
    GD::ipcClient->stop();
    GD::ipcClient.reset();
    BaseLib::ProcessManager::stopSignalHandler(GD::bl->threadManager);
    GD::out.printMessage("(Shutdown) => Shutdown complete.");
    fclose(stdout);
    fclose(stderr);
    gnutls_global_deinit();
    gcry_control(GCRYCTL_SUSPEND_SECMEM_WARN);
    gcry_control(GCRYCTL_TERM_SECMEM);
    gcry_control(GCRYCTL_RESUME_SECMEM_WARN);

    return;
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  _exit(1);
}

void signalHandlerThread() {
  sigset_t set{};
  int signalNumber = -1;
  sigemptyset(&set);
  sigaddset(&set, SIGHUP);
  sigaddset(&set, SIGTERM);
  sigaddset(&set, SIGINT);
  sigaddset(&set, SIGABRT);
  sigaddset(&set, SIGSEGV);
  sigaddset(&set, SIGQUIT);
  sigaddset(&set, SIGILL);
  sigaddset(&set, SIGFPE);
  sigaddset(&set, SIGALRM);
  sigaddset(&set, SIGUSR1);
  sigaddset(&set, SIGUSR2);
  sigaddset(&set, SIGTSTP);
  sigaddset(&set, SIGTTIN);
  sigaddset(&set, SIGTTOU);

  while (!_stopProgram) {
    try {
      if (sigwait(&set, &signalNumber) != 0) {
        GD::out.printError("Error calling sigwait. Killing myself.");
        kill(getpid(), SIGKILL);
      }
      if (signalNumber == SIGTERM || signalNumber == SIGINT) {
        std::unique_lock<std::mutex> stopHomegearGuard(_stopProgramMutex);
        _stopProgram = true;
        _signalNumber = signalNumber;
        stopHomegearGuard.unlock();
        _stopProgramConditionVariable.notify_all();
        return;
      } else if (signalNumber == SIGHUP) {
        GD::out.printMessage("Info: SIGHUP received. Reloading...");

        if (!std::freopen((GD::settings.logfilePath() + "homegear-management.log").c_str(), "a", stdout)) {
          GD::out.printError("Error: Could not redirect output to new log file.");
        }
        if (!std::freopen((GD::settings.logfilePath() + "homegear-management.err").c_str(), "a", stderr)) {
          GD::out.printError("Error: Could not redirect errors to new log file.");
        }

        GD::out.printInfo("Info: Reload complete.");
      } else {
        GD::out.printCritical("Signal " + std::to_string(signalNumber) + " received.");
        pthread_sigmask(SIG_SETMASK, &BaseLib::SharedObjects::defaultSignalMask, nullptr);
        kill(getpid(), signalNumber); //Raise same signal again using the default action.
      }
    }
    catch (const std::exception &ex) {
      GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
  }
}

void getExecutablePath(int argc, char *argv[]) {
  char path[1024];
  if (!getcwd(path, sizeof(path))) {
    std::cerr << "Could not get working directory." << std::endl;
    exit(1);
  }
  GD::workingDirectory = std::string(path);
#ifdef KERN_PROC //BSD system
  int mib[4];
  mib[0] = CTL_KERN;
  mib[1] = KERN_PROC;
  mib[2] = KERN_PROC_PATHNAME;
  mib[3] = -1;
  size_t cb = sizeof(path);
  int result = sysctl(mib, 4, path, &cb, NULL, 0);
  if(result == -1)
  {
      std::cerr << "Could not get executable path." << std::endl;
      exit(1);
  }
  path[sizeof(path) - 1] = '\0';
  GD::executablePath = std::string(path);
  GD::executablePath = GD::executablePath.substr(0, GD::executablePath.find_last_of("/") + 1);
#else
  int length = readlink("/proc/self/exe", path, sizeof(path) - 1);
  if (length < 0) {
    std::cerr << "Could not get executable path." << std::endl;
    exit(1);
  }
  if ((unsigned)length > sizeof(path)) {
    std::cerr << "The path the homegear binary is in has more than 1024 characters." << std::endl;
    exit(1);
  }
  path[length] = '\0';
  GD::executablePath = std::string(path);
  GD::executablePath = GD::executablePath.substr(0, GD::executablePath.find_last_of('/') + 1);
#endif

  GD::executableFile = std::string(argc > 0 ? argv[0] : "homegear");
  BaseLib::HelperFunctions::trim(GD::executableFile);
  if (GD::executableFile.empty()) GD::executableFile = "homegear";
  std::pair<std::string, std::string> pathNamePair = BaseLib::HelperFunctions::splitLast(GD::executableFile, '/');
  if (!pathNamePair.second.empty()) GD::executableFile = pathNamePair.second;
}

void initGnuTls() {
  // {{{ Init gcrypt and GnuTLS
  gcry_error_t gcryResult;
  if ((gcryResult = gcry_control(GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread)) != GPG_ERR_NO_ERROR) {
    GD::out.printCritical("Critical: Could not enable thread support for gcrypt.");
    exit(2);
  }

  if (!gcry_check_version(GCRYPT_VERSION)) {
    GD::out.printCritical("Critical: Wrong gcrypt version.");
    exit(2);
  }
  gcry_control(GCRYCTL_SUSPEND_SECMEM_WARN);
  if ((gcryResult = gcry_control(GCRYCTL_INIT_SECMEM, (int)GD::settings.secureMemorySize(), 0)) != GPG_ERR_NO_ERROR) {
    GD::out.printCritical(
        "Critical: Could not allocate secure memory. Error code is: " + std::to_string((int32_t)gcryResult));
    exit(2);
  }
  gcry_control(GCRYCTL_RESUME_SECMEM_WARN);
  gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);

  int32_t gnutlsResult = 0;
  if ((gnutlsResult = gnutls_global_init()) != GNUTLS_E_SUCCESS) {
    GD::out.printCritical("Critical: Could not initialize GnuTLS: " + std::string(gnutls_strerror(gnutlsResult)));
    exit(2);
  }
  // }}}
}

void setLimits() {
  struct rlimit limits;
  if (!GD::settings.enableCoreDumps()) prctl(PR_SET_DUMPABLE, 0);
  else {
    //Set rlimit for core dumps
    getrlimit(RLIMIT_CORE, &limits);
    limits.rlim_cur = limits.rlim_max;
    GD::out.printInfo(
        "Info: Setting allowed core file size to \"" + std::to_string(limits.rlim_cur) + "\" for user with id "
            + std::to_string(getuid()) + " and group with id " + std::to_string(getgid()) + '.');
    setrlimit(RLIMIT_CORE, &limits);
    getrlimit(RLIMIT_CORE, &limits);
    GD::out.printInfo("Info: Core file size now is \"" + std::to_string(limits.rlim_cur) + "\".");
  }
}

void printHelp() {
  std::cout << "Usage: homegear-management [OPTIONS]" << std::endl << std::endl;
  std::cout << "Option              Meaning" << std::endl;
  std::cout << "-h                  Show this help" << std::endl;
  std::cout << "-u                  Run as user" << std::endl;
  std::cout << "-g                  Run as group" << std::endl;
  std::cout << "-c <path>           Specify path to config file" << std::endl;
  std::cout << "-d                  Run as daemon" << std::endl;
  std::cout << "-p <pid path>       Specify path to process id file" << std::endl;
  std::cout << "-v                  Print program version" << std::endl;
}

void startDaemon() {
  try {
    pid_t pid, sid;
    pid = fork();
    if (pid < 0) {
      exit(1);
    }
    if (pid > 0) {
      exit(0);
    }

    //Set process permission
    umask(S_IWGRP | S_IWOTH);

    //Set child processe's id
    sid = setsid();
    if (sid < 0) {
      exit(1);
    }

    close(STDIN_FILENO);
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
}

void startUp() {
  try {
    if ((chdir(GD::settings.workingDirectory().c_str())) < 0) {
      GD::out.printError("Could not change working directory to " + GD::settings.workingDirectory() + ".");
      exit(1);
    }

    if (!std::freopen((GD::settings.logfilePath() + "homegear-management.log").c_str(), "a", stdout)) {
      GD::out.printError("Error: Could not redirect output to log file.");
    }
    if (!std::freopen((GD::settings.logfilePath() + "homegear-management.err").c_str(), "a", stderr)) {
      GD::out.printError("Error: Could not redirect errors to log file.");
    }

    GD::out.printMessage("Starting Homegear Management...");

    if (GD::settings.memoryDebugging())
      mallopt(M_CHECK_ACTION,
              3); //Print detailed error message, stack trace, and memory, and abort the program. See: http://man7.org/linux/man-pages/man3/mallopt.3.html

    initGnuTls();

    setLimits();

    if (GD::runAsUser.empty()) GD::runAsUser = GD::settings.runAsUser();
    if (GD::runAsGroup.empty()) GD::runAsGroup = GD::settings.runAsGroup();
    if ((!GD::runAsUser.empty() && GD::runAsGroup.empty()) || (!GD::runAsGroup.empty() && GD::runAsUser.empty())) {
      GD::out.printCritical("Critical: You only provided a user OR a group for Homegear to run as. Please specify both.");
      exit(1);
    }
    uid_t userId = GD::bl->hf.userId(GD::runAsUser);
    gid_t groupId = GD::bl->hf.groupId(GD::runAsGroup);
    std::string currentPath;
    if (!GD::pidfilePath.empty() && GD::pidfilePath.find('/') != std::string::npos) {
      currentPath = GD::pidfilePath.substr(0, GD::pidfilePath.find_last_of('/'));
      if (!currentPath.empty()) {
        if (!BaseLib::Io::directoryExists(currentPath)) {
          BaseLib::Io::createDirectory(currentPath, S_IRWXU | S_IRWXG);
          if (chown(currentPath.c_str(), userId, groupId) == -1)
            std::cerr << "Could not set owner on " << currentPath << std::endl;
          if (chmod(currentPath.c_str(), S_IRWXU | S_IRWXG) == -1)
            std::cerr << "Could not set permissions on " << currentPath << std::endl;
        }
      }
    }

    if (getuid() == 0 && !GD::runAsUser.empty() && !GD::runAsGroup.empty()) {
      if (GD::bl->userId == 0 || GD::bl->groupId == 0) {
        GD::out.printCritical("Could not drop privileges. User name or group name is not valid.");
        exit(1);
      }
      GD::out.printInfo(
          "Info: Dropping privileges to user " + GD::runAsUser + " (" + std::to_string(GD::bl->userId) + ") and group "
              + GD::runAsGroup + " (" + std::to_string(GD::bl->groupId) + ")");

      int result = -1;
      std::vector<gid_t> supplementaryGroups(10);
      int numberOfGroups = 10;
      while (result == -1) {
        result = getgrouplist(GD::runAsUser.c_str(), 10000, supplementaryGroups.data(), &numberOfGroups);

        if (result == -1) supplementaryGroups.resize(numberOfGroups);
        else supplementaryGroups.resize(result);
      }

      if (setgid(GD::bl->groupId) != 0) {
        GD::out.printCritical("Critical: Could not drop group privileges.");
        exit(1);
      }

      if (setgroups(supplementaryGroups.size(), supplementaryGroups.data()) != 0) {
        GD::out.printCritical("Critical: Could not set supplementary groups: " + std::string(strerror(errno)));
        exit(1);
      }

      if (setuid(GD::bl->userId) != 0) {
        GD::out.printCritical("Critical: Could not drop user privileges.");
        exit(1);
      }

      //Core dumps are disabled by setuid. Enable them again.
      if (GD::settings.enableCoreDumps()) prctl(PR_SET_DUMPABLE, 1);
    }

    if (getuid() == 0) {
      if (!GD::runAsUser.empty() && !GD::runAsGroup.empty()) {
        GD::out.printCritical(
            "Critical: Homegear still has root privileges though privileges should have been dropped. Exiting Homegear as this is a security risk.");
        exit(1);
      }
    } else {
      if (setuid(0) != -1) {
        GD::out.printCritical(
            "Critical: Regaining root privileges succeded. Exiting Homegear as this is a security risk.");
        exit(1);
      }
      GD::out.printInfo("Info: Homegear Management is (now) running as user with id " + std::to_string(getuid())
                            + " and group with id " + std::to_string(getgid()) + '.');
    }

    //Create PID file
    try {
      if (!GD::pidfilePath.empty()) {
        int32_t pidfile = open(GD::pidfilePath.c_str(), O_CREAT | O_RDWR, 0666);
        if (pidfile < 0) {
          GD::out.printError("Error: Cannot create pid file \"" + GD::pidfilePath + "\".");
        } else {
          int32_t rc = flock(pidfile, LOCK_EX | LOCK_NB);
          if (rc && errno == EWOULDBLOCK) {
            GD::out.printError("Error: Homegear Management is already running - Can't lock PID file.");
          }
          std::string pid(std::to_string(getpid()));
          int32_t bytesWritten = write(pidfile, pid.c_str(), pid.size());
          if (bytesWritten <= 0) GD::out.printError("Error writing to PID file: " + std::string(strerror(errno)));
          close(pidfile);
        }
      }
    }
    catch (const std::exception &ex) {
      GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }

    while (BaseLib::HelperFunctions::getTime() < 1000000000000) {
      GD::out.printWarning("Warning: Time is in the past. Waiting for ntp to set the time...");
      std::this_thread::sleep_for(std::chrono::milliseconds(10000));
    }

    GD::ipcClient.reset(new IpcClient(GD::settings.socketPath() + "homegearIPC.sock"));
    GD::ipcClient->start();

    BaseLib::ProcessManager::startSignalHandler(GD::bl->threadManager); //Needs to be called before starting any threads
    GD::bl->threadManager.start(_signalHandlerThread, true, &signalHandlerThread);

    GD::out.printMessage("Startup complete.");

    GD::bl->booting = false;

    if (BaseLib::Io::fileExists(GD::settings.workingDirectory() + "core")) {
      GD::out.printError(
          "Error: A core file exists in Homegear Management's working directory (\"" + GD::settings.workingDirectory()
              + "core"
              + "\"). Please send this file to the Homegear team including information about your system (Linux distribution, CPU architecture), the Homegear Management version, the current log files and information what might've caused the error.");
    }

    while (!_stopProgram) {
      std::unique_lock<std::mutex> stopHomegearGuard(_stopProgramMutex);
      _stopProgramConditionVariable.wait(stopHomegearGuard);
    }

    terminateProgram(_signalNumber);
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
}

int main(int argc, char *argv[]) {
  try {
    getExecutablePath(argc, argv);
    GD::bl.reset(new BaseLib::SharedObjects());
    GD::out.init(GD::bl.get());

    if (BaseLib::Io::directoryExists(GD::executablePath + "config")) GD::configPath = GD::executablePath + "config/";
    else if (BaseLib::Io::directoryExists(GD::executablePath + "cfg")) GD::configPath = GD::executablePath + "cfg/";
    else GD::configPath = "/etc/homegear/";

    if (std::string(VERSION) != GD::bl->version()) {
      GD::out.printCritical(
          std::string("Base library has wrong version. Expected version ") + VERSION + " but got version "
              + GD::bl->version());
      exit(1);
    }

    for (int32_t i = 1; i < argc; i++) {
      std::string arg(argv[i]);
      if (arg == "-h" || arg == "--help") {
        printHelp();
        exit(0);
      } else if (arg == "-c") {
        if (i + 1 < argc) {
          std::string configPath = std::string(argv[i + 1]);
          if (!configPath.empty()) GD::configPath = configPath;
          if (GD::configPath[GD::configPath.size() - 1] != '/') GD::configPath.push_back('/');
          i++;
        } else {
          printHelp();
          exit(1);
        }
      } else if (arg == "-p") {
        if (i + 1 < argc) {
          GD::pidfilePath = std::string(argv[i + 1]);
          i++;
        } else {
          printHelp();
          exit(1);
        }
      } else if (arg == "-u") {
        if (i + 1 < argc) {
          GD::runAsUser = std::string(argv[i + 1]);
          i++;
        } else {
          printHelp();
          exit(1);
        }
      } else if (arg == "-g") {
        if (i + 1 < argc) {
          GD::runAsGroup = std::string(argv[i + 1]);
          i++;
        } else {
          printHelp();
          exit(1);
        }
      } else if (arg == "-d") {
        _startAsDaemon = true;
      } else if (arg == "-v") {
        std::cout << "Homegear Management version " << VERSION << std::endl;
        std::cout << "Copyright (c) 2013-2018 Homegear GmbH" << std::endl << std::endl;
        exit(0);
      } else {
        printHelp();
        exit(1);
      }
    }

    {
      //Block the signals below during start up
      //Needs to be called after initialization of GD::bl as GD::bl reads the current (default) signal mask.
      sigset_t set{};
      sigemptyset(&set);
      sigaddset(&set, SIGHUP);
      sigaddset(&set, SIGTERM);
      sigaddset(&set, SIGINT);
      sigaddset(&set, SIGABRT);
      sigaddset(&set, SIGSEGV);
      sigaddset(&set, SIGQUIT);
      sigaddset(&set, SIGILL);
      sigaddset(&set, SIGFPE);
      sigaddset(&set, SIGALRM);
      sigaddset(&set, SIGUSR1);
      sigaddset(&set, SIGUSR2);
      sigaddset(&set, SIGTSTP);
      sigaddset(&set, SIGTTIN);
      sigaddset(&set, SIGTTOU);
      if (pthread_sigmask(SIG_BLOCK, &set, nullptr) < 0) {
        std::cerr << "SIG_BLOCK error." << std::endl;
        exit(1);
      }
    }

    // {{{ Load settings
    GD::out.printInfo("Loading settings from " + GD::configPath + "management.conf");
    GD::settings.load(GD::configPath + "management.conf", GD::executablePath);
    GD::bl->settings.load(GD::configPath + "main.conf", GD::executablePath);
    if (GD::runAsUser.empty()) GD::runAsUser = GD::settings.runAsUser();
    if (GD::runAsGroup.empty()) GD::runAsGroup = GD::settings.runAsGroup();
    if ((!GD::runAsUser.empty() && GD::runAsGroup.empty()) || (!GD::runAsGroup.empty() && GD::runAsUser.empty())) {
      GD::out.printCritical(
          "Critical: You only provided a user OR a group for Homegear Management to run as. Please specify both.");
      exit(1);
    }
    GD::bl->userId = GD::bl->hf.userId(GD::runAsUser);
    GD::bl->groupId = GD::bl->hf.groupId(GD::runAsGroup);
    if ((int32_t)GD::bl->userId == -1 || (int32_t)GD::bl->groupId == -1) {
      GD::bl->userId = 0;
      GD::bl->groupId = 0;
    }
    // }}}

    if ((chdir(GD::settings.workingDirectory().c_str())) < 0) {
      GD::out.printError("Could not change working directory to " + GD::settings.workingDirectory() + ".");
      exit(1);
    }

    if (_startAsDaemon) startDaemon();
    startUp();

    GD::bl->threadManager.join(_signalHandlerThread);
    return 0;
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  _exit(1);
}
