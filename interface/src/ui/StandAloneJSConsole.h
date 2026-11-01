//
//  StandAloneJSConsole.h
//
//
//  Created by Clement on 1/17/15.
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_StandAloneJSConsole_h
#define hifi_StandAloneJSConsole_h

#include <QPointer>
#include <DependencyManager.h>

#include "JSConsole.h"

class QDockWidget;

// The scripting console in a dock panel ("Console" in View > Panels, ui/Panels.h): it floats the first time, docks or
// tabs where it's dragged, and comes back where the window's layout has it (dockable panels, step 2)
class StandAloneJSConsole : public QObject, public Dependency {
    Q_OBJECT
    SINGLETON_DEPENDENCY

public slots:
    void toggleConsole();   // opened in front, or closed when it's in front (Panels' toggle)

public:
    // the keyboard is in the console (its window active), so keys don't go to the avatar
    bool getActive();
    // the console's panel, made the first time it's asked for (cheap: the console itself is made as it first shows);
    // none while the sign-in dialog is up
    QDockWidget* dock();
    // the console in it, made now if it hasn't shown yet (for tests)
    JSConsole* console();

private:
    StandAloneJSConsole() {}

    QPointer<QDockWidget> _dock;
};

#endif // hifi_StandAloneJSConsole_h