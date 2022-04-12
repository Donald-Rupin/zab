# Changelog

All notable changes to this project are documented in this file.

See the [Keep a Changelog formatting convention](https://keepachangelog.com/en/1.0.0/) for a guide on contributing to this file, as well as their [example changelog](https://github.com/olivierlacan/keep-a-changelog/blob/master/CHANGELOG.md) to see what makes a good changelog file.

## Unreleased
-  Convert wait_for and first_off to work with any awaitable.
## v0.0.1.0 2022/3/22
### Added

- `generic_awaitable` for creating awaitable types out of lambdas. 
- Added lots of documentation.

### Changed
- Event loops is now liburing based.
- Updated most IO to use `generic_awaitable` and simplified the interface a little.
- event_loop io is now done through an io_ptr instead of a pause_pack*. An io_ptr can represent three different ways to resume and or execute after io completion.

### Fixed
- Timing services issues with polling to regularly. 
- Lots of spelling mistakes.

### Removed
- Fixed buffer io registration. Implementation was messy and was included to just test out. Will think of a better way to do this in the future. 

## v0.0.0.2 2021/1/26

### Added

- This change log!
- `zab::for_each` for easily iterating over `zab::reusable_future` results. 
- `zab::first_of` for returning when the first future resolves. This is thread safe. 

### Changed
- Made a simpler echo server and moved current one to `logging_echo_server.cpp`
- All timers are now performed by `timing_service`.
- order_t is now relative (in nano-seconds) and no longer supports negative ordering.
- engine is now powered by io_uring
### Fixed
- Bug with event loop not picking the least busy loop in `kAnyThread`.
- Bug with timer service not resetting time check cadence. 

### Removed
- Event loop test since there is no ordering to test now.


