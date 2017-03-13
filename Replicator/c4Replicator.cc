//
//  c4Replicator.cc
//  LiteCore
//
//  Created by Jens Alfke on 2/17/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#include "FleeceCpp.hh"
#include "c4.hh"
#include "c4Private.h"
#include "c4Replicator.h"
#include "LibWSProvider.hh"
#include "Replicator.hh"
#include "StringUtil.hh"
#include <atomic>

using namespace std;
using namespace fleece;
using namespace fleeceapi;
using namespace litecore;
using namespace litecore::repl;
using namespace litecore::websocket;


struct C4Replicator : public RefCounted, Replicator::Delegate {

    C4Replicator(C4Database* db,
                 C4Address c4addr,
                 C4ReplicatorMode push,
                 C4ReplicatorMode pull,
                 C4ReplicatorStateChangedCallback onStateChanged,
                 void *callbackContext)
    {
        static websocket::LibWSProvider sWSProvider;

        websocket::Address address(asstring(c4addr.scheme),
                                   asstring(c4addr.hostname),
                                   c4addr.port,
                                   format("/%.*s/_blipsync", SPLAT(c4addr.databaseName)));
        _onStateChanged = onStateChanged;
        _callbackContext = callbackContext;
        _replicator = new Replicator(db, sWSProvider, address, *this, { push, pull });
        _state = {_replicator->activityLevel(), {}};
        sWSProvider.startEventLoop();
        retain(this); // keep myself alive till replicator closes
    }

    C4ReplicatorState state() const     {return _state;}

    void stop()                         {_replicator->stop();}

    void detach()                       {_onStateChanged = nullptr;}

private:

    virtual void replicatorActivityChanged(Replicator*, Replicator::ActivityLevel level) override {
        if (setActivityLevel(level))
            notify();
    }

    virtual void replicatorConnectionClosed(Replicator*,
                                              const Replicator::CloseStatus &status) override
    {
        static const C4ErrorDomain kDomainForReason[] = {WebSocketDomain, POSIXDomain, DNSDomain};

        C4ReplicatorState state = _state;
        if (status.reason == kWebSocketClose && (status.code == kCodeNormal
                                                 || status.code == kCodeGoingAway)) {
            state.error = {};
        } else {
            state.error = c4error_make(kDomainForReason[status.reason], status.code, status.message);
        }
        state.level = kC4Stopped;
        _state = state;
        notify();
        release(this); // balances retain in constructor
    }

    bool setActivityLevel(C4ReplicatorActivityLevel level) {
        C4ReplicatorState state = _state;
        if (state.level == level)
            return false;
        state.level = level;
        _state = state;
        return true;
    }

    
    void notify() {
        C4ReplicatorStateChangedCallback on = _onStateChanged;
        if (on)
            on(this, _state, _callbackContext);
    }

    Retained<Replicator> _replicator;
    atomic<C4ReplicatorState> _state;
    atomic<C4ReplicatorStateChangedCallback> _onStateChanged;
    void *_callbackContext;
};


C4Replicator* c4repl_new(C4Database* db,
                         C4Address c4addr,
                         C4ReplicatorMode push,
                         C4ReplicatorMode pull,
                         C4ReplicatorStateChangedCallback onStateChanged,
                         void *callbackContext,
                         C4Error *err) C4API
{
    try {
        return retain(new C4Replicator(db, c4addr, push, pull, onStateChanged, callbackContext));
    } catch (const std::exception &x) {
        WarnError("Exception caught in c4repl_new");    //FIX: Set *err
        if (err)
            *err = c4error_make(LiteCoreDomain, kC4ErrorUnexpectedError, slice(x.what()));
        // TODO: Return a better error
        return nullptr;
    }
}


void c4repl_stop(C4Replicator* repl) C4API {
    repl->stop();
}


void c4repl_free(C4Replicator* repl) C4API {
    if (!repl)
        return;
    repl->stop();
    repl->detach();
    release(repl);
}


C4ReplicatorState c4repl_getState(C4Replicator *repl) C4API {
    return repl->state();
}
