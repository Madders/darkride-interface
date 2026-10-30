//
//  AutoRigApp.h
//  tools/auto-rig/src
//
//  Created on 2026-10-12 for Darkride's avatar rigger (AUTO-RIGGING.md in the project root).
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_AutoRigApp_h
#define hifi_AutoRigApp_h

#include <QCoreApplication>

// The client's auto-rigger on the command line: rigs a character and measures the joints against an answer key.
class AutoRigApp : public QCoreApplication {
    Q_OBJECT
public:
    AutoRigApp(int argc, char* argv[]);
    ~AutoRigApp();

    int getReturnCode() const { return _returnCode; }

private:
    int _returnCode { 0 };
};

#endif // hifi_AutoRigApp_h
