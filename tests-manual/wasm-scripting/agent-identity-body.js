//
//  agent-identity-body.js
//  tests-manual/wasm-scripting
//
//  The body of agent-identity.sh: an agent that becomes an avatar and sets its model in its first lines, once, and
//  never again (its name too, which shows only for a signed-in agent: an avatar's display name is its account's).
//  Before the fix its identity went out before the avatar mixer's socket was up and was lost, so the mixer showed it
//  to nobody (skunkworks' duck: a faint ghost).
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

Agent.isAvatar = true;
Avatar.displayName = "identity-check";
Avatar.skeletonModelURL = Script.resolvePath("../../interface/resources/meshes/defaultAvatar_full.fst");
print("AGENT-IDENTITY body set its identity at " + Date.now());
