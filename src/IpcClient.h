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

    class CommandInfo
    {
    public:
        int64_t endTime = 0;
        std::string command;
        std::atomic_bool running;
        std::thread thread;
        std::mutex outputMutex;
        std::string output;
        std::atomic_int status;
        Ipc::PVariable metadata;

        CommandInfo()
        {
            running = false;
            status = -1;
        }
    };
    typedef std::shared_ptr<CommandInfo> PCommandInfo;

    std::atomic_bool _disposing;
    std::atomic_bool _rootIsReadOnly;
    std::mutex _commandInfoMutex;
    int32_t _currentCommandInfoId = 0;
    std::unordered_map<int32_t, PCommandInfo> _commandInfo;
    std::mutex _readOnlyCountMutex;
    int32_t _readOnlyCount = 0;

    int32_t startCommandThread(std::string command, Ipc::PVariable metadata = std::make_shared<Ipc::Variable>());
    void executeCommand(PCommandInfo commandInfo);

    void setRootReadOnly(bool readOnly);

    // {{{ RPC methods
    Ipc::PVariable dpkgPackageInstalled(Ipc::PArray& parameters);
    Ipc::PVariable getCommandStatus(Ipc::PArray& parameters);
    Ipc::PVariable sleep(Ipc::PArray& parameters);
    Ipc::PVariable getConfigurationEntry(Ipc::PArray& parameters);
    Ipc::PVariable reboot(Ipc::PArray& parameters);
    Ipc::PVariable serviceCommand(Ipc::PArray& parameters);
    Ipc::PVariable setConfigurationEntry(Ipc::PArray& parameters);
    Ipc::PVariable writeCloudMaticConfig(Ipc::PArray& parameters);
    // }}}

    // {{{ User management
    Ipc::PVariable setUserPassword(Ipc::PArray& parameters);
    // }}}

    // {{{ Updates
    Ipc::PVariable aptUpdate(Ipc::PArray& parameters);
    Ipc::PVariable aptUpgrade(Ipc::PArray& parameters);
    Ipc::PVariable aptFullUpgrade(Ipc::PArray& parameters);
    Ipc::PVariable homegearUpdateAvailable(Ipc::PArray& parameters);
    Ipc::PVariable systemUpdateAvailable(Ipc::PArray& parameters);
    // }}}

    // {{{ Backups
    Ipc::PVariable createBackup(Ipc::PArray& parameters);
    Ipc::PVariable restoreBackup(Ipc::PArray& parameters);
    // }}}

    // {{{ CA and gateways
    Ipc::PVariable caExists(Ipc::PArray& parameters);
    Ipc::PVariable createCa(Ipc::PArray& parameters);
    Ipc::PVariable createCert(Ipc::PArray& parameters);
    Ipc::PVariable deleteCert(Ipc::PArray& parameters);
    // }}}

    // {{{ System configuration
        /**
         * Parses and returns the content of the file `/etc/network/interfaces` and the Homegear section of `/etc/resolvconf/resolv.conf.d/head`.
         *
         * The returned Struct looks like this:
         *
         *     {
         *         "eth0": {
         *             "ipv4": {
         *                 type: "static", // or type: "dhcp"
         *                 address: "192.168.178.5",
         *                 subnet: "255.255.255.0"
         *             },
         *             "ipv6": {
         *                 type: "auto", // or type: "static"
         *                 address: "fdab:1::5",
         *                 subnet: "64"
         *             },
         *             "dns": ["9.9.9.9", "1.1.1.1", "2620:fe::fe"] // Leave emtpy for automatic
         *         }
         *     }
         *
         * @param parameters This method has no parameters.
         * @return Returns the content of the file `/etc/network/interfaces` as a Struct.
         */
        Ipc::PVariable getNetworkConfiguration(Ipc::PArray& parameters);

        /**
         * Sets the content of the file `/etc/network/interfaces`. You need to provice a Struct with the new configuration as parameter:
         *
         *     {
         *         "eth0": {
         *             "ipv4": {
         *                 type: "static", // or type: "dhcp"
         *                 address: "192.168.178.5",
         *                 subnet: "255.255.255.0"
         *             },
         *             "ipv6": {
         *                 type: "auto", // or type: "static"
         *                 address: "fdab:1::5",
         *                 subnet: "64"
         *             },
         *             "dns": ["9.9.9.9", "1.1.1.1", "2620:fe::fe"] // Leave emtpy for automatic
         *         }
         *     }
         *
         * @param parameters The new network configuration as a Struct.
         * @return Returns Void on success.
         */
        Ipc::PVariable setNetworkConfiguration(Ipc::PArray& parameters);
    // }}}

    // {{{ Device description files
    Ipc::PVariable copyDeviceDescriptionFile(Ipc::PArray& parameters);
    // }}}
};

#endif
