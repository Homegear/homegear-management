/* Copyright 2013-2017 Sathya Laufer
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

#ifndef IPCCLIENT_H_
#define IPCCLIENT_H_

#include <homegear-base/BaseLib.h>
#include <homegear-ipc/IIpcClient.h>

#include <thread>
#include <mutex>
#include <string>
#include <set>

class IpcClient : public Ipc::IIpcClient
{
public:
    IpcClient(std::string socketPath);
    virtual ~IpcClient();
private:
    virtual void onConnect();

    std::atomic_bool _commandThreadRunning;
    std::thread _commandThread;
    std::mutex _commandOutputMutex;
    std::atomic_int _commandStatus;
    std::string _commandOutput;

    void executeCommand(std::string command);

    // {{{ RPC methods
    Ipc::PVariable aptUpdate(Ipc::PArray& parameters);
    Ipc::PVariable aptUpgrade(Ipc::PArray& parameters);
    Ipc::PVariable aptFullUpgrade(Ipc::PArray& parameters);
    Ipc::PVariable dpkgPackageInstalled(Ipc::PArray& parameters);
    Ipc::PVariable getCommandStatus(Ipc::PArray& parameters);
    Ipc::PVariable getConfigurationEntry(Ipc::PArray& parameters);
    Ipc::PVariable reboot(Ipc::PArray& parameters);
    Ipc::PVariable serviceCommand(Ipc::PArray& parameters);
    Ipc::PVariable setConfigurationEntry(Ipc::PArray& parameters);
    Ipc::PVariable writeCloudMaticConfig(Ipc::PArray& parameters);
    // }}}

    // {{{ Backups
    Ipc::PVariable createBackup(Ipc::PArray& parameters);
    Ipc::PVariable restoreBackup(Ipc::PArray& parameters);
    // }}}

    // {{{ CA and gateways
    Ipc::PVariable caExists(Ipc::PArray& parameters);
    Ipc::PVariable createCa(Ipc::PArray& parameters);
    // }}}
};

#endif
