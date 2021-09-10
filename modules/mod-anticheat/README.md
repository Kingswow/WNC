# ![logo](https://raw.githubusercontent.com/azerothcore/azerothcore.github.io/master/images/logo-github.png) AzerothCore
## Anticheat Module
- Latest build status with azerothcore: [![Build Status](https://github.com/azerothcore/mod-anticheat/workflows/core-build/badge.svg?branch=master&event=push)](https://github.com/Kingswow/WNC/tree/NewAnticheat)

This is a port of the PassiveAnticheat Script from lordpsyan's repo to [AzerothCore](http://www.azerothcore.org)

## How to install

### 1) First Download Master Branch.

`git clone https://github.com/Kingswow/WNC.git AC3.3.5`


### 2) Simply place the module under the `modules` folder of your AzerothCore source folder.

You can do clone it via git under the azerothcore/modules directory:

`cd path/to/azerothcore/modules`

`git clone https://github.com/Kingswow/WNC.git NewAnticheat`

or you can manually [download the module](https://github.com/Kingswow/WNC/archive/refs/heads/NewAnticheat.zip), unzip and place it under the `azerothcore/modules` directory.

### 3) Re-run cmake and launch a clean build of AzerothCore

### 4) Execute the included "mod-anticheat/sql/auth/base/auth_anticheat_logs.sql and mod-anticheat/sql/world/base/world_acore_string.sql" file on your World And Auth databases. This creates the necessary tables for this module.

**That's it.**

### (Optional) Edit module configuration

If you need to change the module configuration, go to your server configuration folder (e.g. **etc**), copy `Anticheat.conf.dist` to `Anticheat.conf` and edit it as you prefer.
