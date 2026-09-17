/* The two things the device does between sleeps. */
#pragma once

void composeLoop();                  // button wake: type on CardKB, Enter sends
void pollSequence(bool firstBoot);   // timer wake / power-on: narrated mailbox check
