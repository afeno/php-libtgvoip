/*
Copyright 2016-2017 Daniil Gentili
(https://daniil.it)
This file is part of php-libtgvoip.
php-libtgvoip is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License as published by the free Software Foundation, either version 3 of the License, or (at your option) any later version.
The PWRTelegram API is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Affero General Public License for more details.
You should have received a copy of the GNU General Public License along with php-libtgvoip.
If not, see <http://www.gnu.org/licenses/>.
*/

#include "main.h"
#include "audio/AudioInputModule.h"
#include "audio/AudioOutputModule.h"
#include <string.h>
#include <wchar.h>
#include <map>
#include <string>
#include <vector>
#include <queue>

#include "libtgvoip/VoIPServerConfig.h"
#include "libtgvoip/threading.h"
#include "libtgvoip/logging.h"

using namespace tgvoip;
using namespace tgvoip::audio;
using namespace std;

void VoIP::__construct()
{
/*
    PHPthis = (Php::Object)this;
    madeline = params[0];
    current_call = params[1];*/
    in=NULL;
    out=NULL;
    inst = new VoIPController();

    inst->implData = (void *) this;
    inst->SetStateCallback([](VoIPController *controller, int state) {
        ((VoIP *)controller->implData)->state = state;
        /*
        if (state == STATE_FAILED) {
            ((VoIP *)controller->implData)->__destruct();
        }*/
    });
}
void VoIP::__destruct()
{
    destroyed = true;
    delete inst;
}

void VoIP::__wakeup()
{
    __construct();
    if (configuration) {
        parseConfig();
    }
    if (proxyConfiguration) {
        configureProxy();
    }
}


void VoIP::start()
{
    inst->Start();
}
void VoIP::connect()
{
    inst->Connect();
}

Php::Value VoIP::getProxy() const
{
    return proxyConfiguration;
}


Php::Value VoIP::getConfig() const
{
    return configuration;
}

void VoIP::setConfig(Php::Parameters &params)
{
    configuration = params[0];
    this->parseConfig();
}

void VoIP::parseConfig() {
    voip_config_t cfg;
    cfg.recv_timeout = configuration["config"]["recv_timeout"];
    cfg.init_timeout = configuration["config"]["init_timeout"];
    cfg.data_saving = configuration["config"]["data_saving"];
    cfg.enableAEC = configuration["config"]["enable_AEC"];
    cfg.enableNS = configuration["config"]["enable_NS"];
    cfg.enableAGC = configuration["config"]["enable_AGC"];

    Php::Value log_file_path = "log_file_path";
    if (configuration["config"].contains(log_file_path))
    {
        strncpy(cfg.logFilePath, configuration["config"]["log_file_path"], sizeof(cfg.logFilePath));
        cfg.logFilePath[sizeof(cfg.logFilePath) - 1] = 0;
    }
    else
    {
        memset(cfg.logFilePath, 0, sizeof(cfg.logFilePath));
    }
    Php::Value stats_dump_file_path = "stats_dump_file_path";

    if (configuration["config"].contains(stats_dump_file_path))
    {
        strncpy(cfg.statsDumpFilePath, configuration["config"]["stats_dump_file_path"], sizeof(cfg.statsDumpFilePath));
        cfg.statsDumpFilePath[sizeof(cfg.statsDumpFilePath) - 1] = 0;
    }
    else
    {
        memset(cfg.statsDumpFilePath, 0, sizeof(cfg.statsDumpFilePath));
    }
    inst->SetConfig(&cfg);
    Php::Value shared_config = configuration["shared_config"];
    ServerConfig::GetSharedInstance()->Update(shared_config);

    char *key = (char *) malloc(256);
    memcpy(key, configuration["auth_key"], 256);
    inst->SetEncryptionKey(key, (bool) configuration["outgoing"]);
    free(key);

    vector<Endpoint> eps;
    Php::Value endpoints = configuration["endpoints"];
    for (int i = 0; i < endpoints.size(); i++)
    {
        string ip = endpoints[i]["ip"];
        string ipv6 = endpoints[i]["ipv6"];
        string peer_tag = endpoints[i]["peer_tag"];

        IPv4Address v4addr(ip);
        IPv6Address v6addr("::0");
        unsigned char *pTag = (unsigned char *) malloc(16);

        if (ipv6 != "")
        {
            v6addr = IPv6Address(ipv6);
        }

        if (peer_tag != "")
        {
            memcpy(pTag, peer_tag.c_str(), 16);
        }

        eps.push_back(Endpoint(endpoints[i]["id"], (int32_t)endpoints[i]["port"], v4addr, v6addr, EP_TYPE_UDP_RELAY, pTag));
        free(pTag);
    }
    
    inst->SetRemoteEndpoints(eps, configuration["allow_p2p"]);
    inst->SetNetworkType(configuration["network_type"]);

}

Php::Value VoIP::setOutputFile(Php::Parameters &params) {
    return out->setOutputFile(params[0]);
}
Php::Value VoIP::unsetOutputFile() {
    return out->unsetOutputFile();
}
Php::Value VoIP::play(Php::Parameters &params) {
    if (in->play(params[0])) {
        return this;
    }
    return false;
}
Php::Value VoIP::playOnHold(Php::Parameters &params) {
    return in->playOnHold(params);
}

void VoIP::setMicMute(Php::Parameters &params)
{
    inst->SetMicMute(params[0]);
}
// int protocol, string address, uint16_t port, string username, string password
void VoIP::setProxy(Php::Parameters &params)
{
    proxyConfiguration = params[0];
    configureProxy();
}

void VoIP::configureProxy()
{
    inst->SetProxy(proxyConfiguration["protocol"], proxyConfiguration["address"], (int32_t) proxyConfiguration["port"], proxyConfiguration["username"], proxyConfiguration["password"]);
}

void VoIP::debugCtl(Php::Parameters &params)
{
    inst->DebugCtl(params[0], params[1]);
}

Php::Value VoIP::getDebugLog()
{
    return inst->GetDebugLog();
}

Php::Value VoIP::getVersion()
{
    return VoIPController::GetVersion();
}

Php::Value VoIP::getPreferredRelayID()
{
    return inst->GetPreferredRelayID();
}

Php::Value VoIP::getLastError()
{
    return inst->GetLastError();
}
Php::Value VoIP::getDebugString()
{
    char *buf = (char *) malloc(10240);
    inst->GetDebugString(buf, 10240);
    Php::Value returnvalue = buf;
    free(buf);
    return returnvalue;
}
Php::Value VoIP::getStats()
{
    voip_stats_t _stats;
    inst->GetStats(&_stats);
    Php::Value stats;
    stats["bytesSentWifi"] = (int64_t)_stats.bytesSentWifi;
    stats["bytesSentMobile"] = (int64_t)_stats.bytesSentMobile;
    stats["bytesRecvdWifi"] = (int64_t)_stats.bytesRecvdWifi;
    stats["bytesRecvdMobile"] = (int64_t)_stats.bytesRecvdMobile;
    return stats;
}


void VoIP::setOutputLevel(Php::Parameters &params) {
    out->outputLevel = (double) params[0];
}

Php::Value VoIP::getState()
{
    return state;
}

Php::Value VoIP::isDestroyed()
{
    return destroyed;
}

Php::Value VoIP::isPlaying()
{
    return playing;
}

Php::Value VoIP::getOutputState()
{
    return outputState;
}

Php::Value VoIP::getInputState()
{
    return inputState;
}

Php::Value VoIP::getOutputParams()
{
    Php::Value params;
    params["bitsPerSample"] = out->outputBitsPerSample;
    params["sampleRate"] = out->outputSampleRate;
    params["channels"] = out->outputChannels;
    params["samplePeriod"] = out->outputSamplePeriod;
    params["writePeriod"] = out->outputWritePeriod;
    params["sampleNumber"] = out->outputSampleNumber;
    params["samplesSize"] = out->outputSamplesSize;
    params["level"] = out->outputLevel;

    return params;

}

Php::Value VoIP::getInputParams()
{
    Php::Value params;
    params["bitsPerSample"] = in->inputBitsPerSample;
    params["sampleRate"] = in->inputSampleRate;
    params["channels"] = in->inputChannels;
    params["samplePeriod"] = in->inputSamplePeriod;
    params["writePeriod"] = in->inputWritePeriod;
    params["sampleNumber"] = in->inputSampleNumber;
    params["samplesSize"] = in->inputSamplesSize;

    return params;

}



extern "C" {

/**
     *  Function that is called by PHP right after the PHP process
     *  has started, and that returns an address of an internal PHP
     *  strucure with all the details and features of your extension
     *
     *  @return void*   a pointer to an address that is understood by PHP
     */
PHPCPP_EXPORT void *get_module()
{
    // static(!) Php::Extension object that should stay in memory
    // for the entire duration of the process (that's why it's static)
    static Php::Extension extension("php-libtgvoip", "1.0");

    // description of the class so that PHP knows which methods are accessible
    Php::Class<VoIP> voip("VoIP");

    voip.method<&VoIP::getState>("getState", Php::Public | Php::Final);
    voip.method<&VoIP::isPlaying>("isPlaying", Php::Public | Php::Final);
    voip.method<&VoIP::isDestroyed>("isDestroyed", Php::Public | Php::Final);
    voip.method<&VoIP::getOutputState>("getOutputState", Php::Public | Php::Final);
    voip.method<&VoIP::getInputState>("getInputState", Php::Public | Php::Final);
    voip.method<&VoIP::getOutputParams>("getOutputParams", Php::Public | Php::Final);
    voip.method<&VoIP::getInputParams>("getInputParams", Php::Public | Php::Final);

    voip.method<&VoIP::__destruct>("__destruct", Php::Public | Php::Final);
    voip.method<&VoIP::__construct>("__construct", Php::Public | Php::Final);
    voip.method<&VoIP::__wakeup>("__wakeup", Php::Public | Php::Final);
    voip.method<&VoIP::setMicMute>("setMicMute", Php::Public | Php::Final, {
        Php::ByVal("type", Php::Type::Bool),
    });
    voip.method<&VoIP::debugCtl>("debugCtl", Php::Public | Php::Final, {
        Php::ByVal("request", Php::Type::Numeric), Php::ByVal("param", Php::Type::Numeric),
    });
    voip.method<&VoIP::setConfig>("setConfig", Php::Public | Php::Final, {
        Php::ByVal("config", Php::Type::Array),
    });
    voip.method<&VoIP::setProxy>("setProxy", Php::Public | Php::Final, {
        Php::ByVal("proxyConfiguration", Php::Type::Array),
    });
    voip.method<&VoIP::getDebugLog>("getDebugLog", Php::Public | Php::Final);
    voip.method<&VoIP::getLastError>("getLastError", Php::Public | Php::Final);
    voip.method<&VoIP::getPreferredRelayID>("getPreferredRelayID", Php::Public | Php::Final);
    voip.method<&VoIP::getVersion>("getVersion", Php::Public | Php::Final);
    voip.method<&VoIP::getDebugString>("getDebugString", Php::Public | Php::Final);
    voip.method<&VoIP::getStats>("getStats", Php::Public | Php::Final);
    voip.method<&VoIP::start>("start", Php::Public | Php::Final);
    voip.method<&VoIP::connect>("connect", Php::Public | Php::Final);
    
    voip.method<&VoIP::play>("then", Php::Public | Php::Final, {Php::ByVal("file", Php::Type::String)});
    voip.method<&VoIP::play>("play", Php::Public | Php::Final, {Php::ByVal("file", Php::Type::String)});
    voip.method<&VoIP::playOnHold>("playOnHold", Php::Public | Php::Final, {Php::ByVal("files", Php::Type::Array)});
    
    voip.method<&VoIP::setOutputFile>("setOutputFile", Php::Public | Php::Final, {Php::ByVal("file", Php::Type::String)});
    voip.method<&VoIP::unsetOutputFile>("unsetOutputFile", Php::Public | Php::Final);


    voip.property("configuration", &VoIP::getConfig, Php::Private);
    voip.property("proxyConfiguration", &VoIP::getProxy, Php::Private);

    voip.constant("STATE_CREATED", STATE_CREATED);
    voip.constant("STATE_WAIT_INIT", STATE_WAIT_INIT);
    voip.constant("STATE_WAIT_INIT_ACK", STATE_WAIT_INIT_ACK);
    voip.constant("STATE_ESTABLISHED", STATE_ESTABLISHED);
    voip.constant("STATE_FAILED", STATE_FAILED);
    voip.constant("STATE_RECONNECTING", STATE_RECONNECTING);

    voip.constant("TGVOIP_ERROR_UNKNOWN", TGVOIP_ERROR_UNKNOWN);
    voip.constant("TGVOIP_ERROR_INCOMPATIBLE", TGVOIP_ERROR_INCOMPATIBLE);
    voip.constant("TGVOIP_ERROR_TIMEOUT", TGVOIP_ERROR_TIMEOUT);
    voip.constant("TGVOIP_ERROR_AUDIO_IO", TGVOIP_ERROR_AUDIO_IO);

    voip.constant("NET_TYPE_UNKNOWN", NET_TYPE_UNKNOWN);
    voip.constant("NET_TYPE_GPRS", NET_TYPE_GPRS);
    voip.constant("NET_TYPE_EDGE", NET_TYPE_EDGE);
    voip.constant("NET_TYPE_3G", NET_TYPE_3G);
    voip.constant("NET_TYPE_HSPA", NET_TYPE_HSPA);
    voip.constant("NET_TYPE_LTE", NET_TYPE_LTE);
    voip.constant("NET_TYPE_WIFI", NET_TYPE_WIFI);
    voip.constant("NET_TYPE_ETHERNET", NET_TYPE_ETHERNET);
    voip.constant("NET_TYPE_OTHER_HIGH_SPEED", NET_TYPE_OTHER_HIGH_SPEED);
    voip.constant("NET_TYPE_OTHER_LOW_SPEED", NET_TYPE_OTHER_LOW_SPEED);
    voip.constant("NET_TYPE_DIALUP", NET_TYPE_DIALUP);
    voip.constant("NET_TYPE_OTHER_MOBILE", NET_TYPE_OTHER_MOBILE);

    voip.constant("DATA_SAVING_NEVER", DATA_SAVING_NEVER);
    voip.constant("DATA_SAVING_MOBILE", DATA_SAVING_MOBILE);
    voip.constant("DATA_SAVING_ALWAYS", DATA_SAVING_ALWAYS);

    voip.constant("PROXY_NONE", PROXY_NONE);
    voip.constant("PROXY_SOCKS5", PROXY_SOCKS5);

    voip.constant("AUDIO_STATE_NONE", AUDIO_STATE_NONE);
    voip.constant("AUDIO_STATE_CREATED", AUDIO_STATE_CREATED);
    voip.constant("AUDIO_STATE_CONFIGURED", AUDIO_STATE_CONFIGURED);
    voip.constant("AUDIO_STATE_RUNNING", AUDIO_STATE_RUNNING);

    Php::Namespace danog("danog");
    Php::Namespace MadelineProto("MadelineProto");
    
    MadelineProto.add(move(voip));
    danog.add(move(MadelineProto));
    extension.add(move(danog));

    return extension;
}
}
