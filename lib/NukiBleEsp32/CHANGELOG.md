# Changelog

## V2.6.2 (Unreleased)

- Improve logging level consistency and error visibility in several methods
- Rate-limit NVS credential error logs to suppress repeated credential error messages
- Assign appropriate log levels (debug/info/warn/error) to selected log messages

## V2.6.1 (Unreleased)

- Fix GitHub build (#90)
- Make timeouts configurable
- Add NoWarnings BatteryType

## V2.6.0 (Unreleased)

- Add SmartLock Ultra support; API 2.3.0 support (#85)
- New commands, C5 support, connection improvements (#89)

## V2.5.0 (Unreleased)

- Compatibility with esp-nimble-cpp >= 2.0, runtime logger/connection-mode/debug configuration (#82)
- Various enhancements and bug fixes (#83)

## V2.4.0 (Unreleased)

- Switch license to MIT
- Fix Continuous Mode / disconnect & reconnect on failed commands (#77)
- Signal KeyTurnerStatusReset (#78)
- esp-nimble-cpp compatibility and optimize disconnect (#79)

## V2.3.0 (Unreleased)

- Add option for recursive mutex (#66)
- Arduino Core 3 and IDF5 compatibility (#68)
- Add option to use 64-bit timer (#70)
- Alternative connect mode; make TX power configurable (#71, #72)
- C6 fix (#73)
- Prevent inappropriate disconnect (#74)
- Add compatibility with esp-nimble-cpp master (#75)

## V2.2.0 (Unreleased)

- Use atomic types for shared variables in onResult (#60)
- Improve pairing reliability and speed (#61)
- Fix authorization entries (#64)

## V2.1.0 (Unreleased)

- Keypad v2 fields + support (#36)
- Logging updates (incl. remove logging pincode) + random nonce improvements (#32)
- KeypadAction support + related fixes (#44, #40, #55)
- Extend config (#51)
- Fix updateTime(): TimeValue struct size (#56)
- Keep pinCode in RAM and update it when setting pinCode (#57)
- Initialize errorCode deterministically (#59)
- Docs: close stale connections / updateConnectionState() (#58)

## V2.0.0 (2023-02-23)

- Increased version number to 2.0.0 (Commit 56b7b2b)
- Updated NimBLE dependency to 1.4.0; compatibility fixes for NimBLE 1.4.x
- Expose last heartbeat value; track last received BLE beacon timestamp
- Add method to get BLE address
- Added missing ContinuousMode value in enum
- Lock-busy handling: map to CmdResult "Lock_Busy"
- Reboot patch when BLE scanner stops unexpectedly (PR #31)
- add lockactionToString() for opener (PR #35)

## V1.0.0 (2022-08-01)

- Increased version to 1.0.0
- Added Nuki Opener support (PR #14)
- Opener configuration improvements (settings + sound level) (PR #16)

## V0.0.10 (2022-11-07)

- Prevented pairing failure when pairing - unpairing - pairing
- Refactored checking credentials (not deleting preference key's anymore)

## V0.0.9 (2022-10-04)

- Prevented executing action when lock is not present (battery or lock failure) based on advertising heartbeat
- Reduced BLE connect retries from 10 to 5  
- Fixed build issue and exception when enabeling debugging
- Added "Nightmode active" and "Accessory Battery State" to keyturner struct data
- Added Nuki Opener support
- Added pairingEnabled method
- Added retreiving and logging of authorization entries  
- Added deleteAuthorizationEntry
- Added getSecurityPincode
- Added getMacAddress
- Fixed not working pairing timeout
- Added lockHeartBeat when pairing successfull

## V0.0.8 (2022-05-18)

- Prevented millis() overflow issue
- Corrected setDisonnectTimeout()

## V0.0.7 (2022-05-17)

- tweaked semaphore timeouts and delays
- added option to check if communication is done and then disconnect ble (saves battery and speeds up getting advertisements)
- improved semaphore logging

## V0.0.6 (2022-05-03)

- Removed local BLE scanner from library to be able to use the same scanner for multiple BLE devices in 1 project. <https://github.com/I-Connect/BleScanner.git> can be used.
- Added/updated header to all files
- Added documentation to all public methods
- Set NimBle version to be used
- Added changelog  

## V0.0.5

- Updated to Espressif platform 4.x.x
- Fixed battery indications
- Some general refactoring

## V0.0.4

- Added semaphores to make it (more) threadsafe
- Fixed handling payload len in lock action in case in case a namesuffix is used
- Some general refactoring

## V0.0.3

- Cleanup and refactor
- Fixed loosing pincode on re-pairing
- Updated scanning intervals according to recommendations Nuki dev
- Made BLE scanner injectable

## V0.0.2

- Added eventhandler

## V0.0.1

lib is ready for beta testing, most if not all Nuki lock v2 functionality is implemented.
Most of the basic methods have been tested, some of the more advanced (mostly settings related) methods still need to be tested
There can still be braking changes....!
Implementation is according to Nuki Smart Lock API V2.2.1 (22.06.2021)
