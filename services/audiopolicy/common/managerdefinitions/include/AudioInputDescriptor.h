/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "AudioIODescriptorInterface.h"
#include "AudioPort.h"
#include "AudioSession.h"
#include "ClientDescriptor.h"
#include <utils/Errors.h>
#include <system/audio.h>
#include <utils/SortedVector.h>
#include <utils/KeyedVector.h>

namespace android {

class IOProfile;
class AudioMix;

// descriptor for audio inputs. Used to maintain current configuration of each opened audio input
// and keep track of the usage of this input.
class AudioInputDescriptor: public AudioPortConfig, public AudioIODescriptorInterface
{
public:
    explicit AudioInputDescriptor(const sp<IOProfile>& profile,
                                  AudioPolicyClientInterface *clientInterface);
    audio_port_handle_t getId() const;
    audio_module_handle_t getModuleHandle() const;
    uint32_t getOpenRefCount() const;

    status_t    dump(int fd);

    audio_io_handle_t             mIoHandle;       // input handle
    audio_devices_t               mDevice;         // current device this input is routed to
    AudioMix                      *mPolicyMix;     // non NULL when used by a dynamic policy
    const sp<IOProfile>           mProfile;        // I/O profile this output derives from

    virtual void toAudioPortConfig(struct audio_port_config *dstConfig,
            const struct audio_port_config *srcConfig = NULL) const;
    virtual sp<AudioPort> getAudioPort() const { return mProfile; }
    void toAudioPort(struct audio_port *port) const;
    void setPreemptedSessions(const SortedVector<audio_session_t>& sessions);
    SortedVector<audio_session_t> getPreemptedSessions() const;
    bool hasPreemptedSession(audio_session_t session) const;
    void clearPreemptedSessions();
    bool isActive() const;
    bool isSourceActive(audio_source_t source) const;
    audio_source_t inputSource(bool activeOnly = false) const;
    bool isSoundTrigger() const;
    status_t addAudioSession(audio_session_t session,
                             const sp<AudioSession>& audioSession);
    status_t removeAudioSession(audio_session_t session);
    sp<AudioSession> getAudioSession(audio_session_t session) const;
    AudioSessionCollection getAudioSessions(bool activeOnly) const;
    size_t getAudioSessionCount(bool activeOnly) const;
    audio_source_t getHighestPrioritySource(bool activeOnly) const;
    void changeRefCount(audio_session_t session, int delta);


    // implementation of AudioIODescriptorInterface
    audio_config_base_t getConfig() const override;
    audio_patch_handle_t getPatchHandle() const override;
    void setPatchHandle(audio_patch_handle_t handle) override;

    status_t open(const audio_config_t *config,
                  audio_devices_t device,
                  const String8& address,
                  audio_source_t source,
                  audio_input_flags_t flags,
                  audio_io_handle_t *input);
    // Called when a stream is about to be started.
    // Note: called after changeRefCount(session, 1)
    status_t start();
    // Called after a stream is stopped
    // Note: called after changeRefCount(session, -1)
    void stop();
    void close();

    RecordClientMap& clients() { return mClients; }
    RecordClientVector getClientsForSession(audio_session_t session);

 private:

    void updateSessionRecordingConfiguration(int event, const sp<AudioSession>& audioSession);

    audio_patch_handle_t          mPatchHandle;
    audio_port_handle_t           mId;
    // audio sessions attached to this input
    AudioSessionCollection        mSessions;
    // Because a preemptible capture session can preempt another one, we end up in an endless loop
    // situation were each session is allowed to restart after being preempted,
    // thus preempting the other one which restarts and so on.
    // To avoid this situation, we store which audio session was preempted when
    // a particular input started and prevent preemption of this active input by this session.
    // We also inherit sessions from the preempted input to avoid a 3 way preemption loop etc...
    SortedVector<audio_session_t> mPreemptedSessions;
    AudioPolicyClientInterface *mClientInterface;
    uint32_t mGlobalRefCount;  // non-session-specific ref count

    RecordClientMap mClients;
};

class AudioInputCollection :
        public DefaultKeyedVector< audio_io_handle_t, sp<AudioInputDescriptor> >
{
public:
    bool isSourceActive(audio_source_t source) const;

    sp<AudioInputDescriptor> getInputFromId(audio_port_handle_t id) const;

    // count active capture sessions using one of the specified devices.
    // ignore devices if AUDIO_DEVICE_IN_DEFAULT is passed
    uint32_t activeInputsCountOnDevices(audio_devices_t devices = AUDIO_DEVICE_IN_DEFAULT) const;

    /**
     * return io handle of active input or 0 if no input is active
     * Only considers inputs from physical devices (e.g. main mic, headset mic) when
     * ignoreVirtualInputs is true.
     */
    Vector<sp <AudioInputDescriptor> > getActiveInputs(bool ignoreVirtualInputs = true);

    audio_devices_t getSupportedDevices(audio_io_handle_t handle) const;

    sp<AudioInputDescriptor> getInputForClient(audio_port_handle_t portId);

    status_t dump(int fd) const;
};


} // namespace android