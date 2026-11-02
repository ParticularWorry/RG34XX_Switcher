#!/bin/sh
# HELP: RG34 Turbo Refresh Switcher
# GRID: RG34 Turbo

. /opt/muos/script/var/func.sh

APP_BIN="rg34xx-tool"
SETUP_APP "$APP_BIN" ""

cd /run/muos/storage/application/RG34XXSwitcher || exit 1
./"$APP_BIN" > app.log 2>&1
