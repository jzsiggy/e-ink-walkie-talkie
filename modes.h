/* What the device does between sleeps, one per wake. */
#pragma once

bool waitForHold();                       // button C wake: true if still held after HOLD_POLL_MS (poll instead)
void composeLoop();                       // button C wake, configured: type on CardKB, Enter sends
void pollSequence(bool firstBoot);        // timer wake / power-on, configured: narrated mailbox check
void setupLoop();                         // button A wake: WiFi setup hotspot
void notConfiguredNotice(bool drawPanel); // any wake while unconfigured: say so (and draw the hint once)
