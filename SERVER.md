# Hosting a T5Jool dedicated server

A dedicated server runs a Zombies match on its own, without graphics or sound, so players can join it from the
**Zombies → Co-op online** list at any time. Up to **4 players** per server.

> T5Jool is in alpha: dedicated servers are new and may be unstable. Please report problems on
> [GitHub issues](https://github.com/Joolace/T5-Jool---Black-Ops-Recomp/issues) with the server log.

## Requirements

- **Linux x86_64** (Ubuntu 22.04 or newer recommended). A home PC or a rented server both work.
- At least **4 GB of RAM** and about **20 GB** of disk space.
- **Your own copy of Call of Duty: Black Ops**: the `zone` and `main` folders and the `localization.txt` file from your PC install (for example
  `steamapps/common/Call of Duty Black Ops`). T5Jool does not include game files.
- Two **UDP ports** reachable from the internet: **3074** (game) and **3075** (server info and ping).

ARM servers (Oracle Ampere, Raspberry Pi) are not supported yet.

## 1. Install

Download `T5Jool-v<version>-linux-x86_64.tar.gz` from the [latest release](https://github.com/Joolace/T5-Jool---Black-Ops-Recomp/releases)
and copy your game files to the server:

```
tar xf T5Jool-v*-linux-x86_64.tar.gz
cd T5Jool-v*-linux-x86_64
mkdir -p ~/blackops
# copy zone/, main/ and localization.txt of your Black Ops install into ~/blackops (scp, rsync, USB...)
```

From your PC, for example:

```
rsync -av --progress "/path/to/Black Ops/zone" "/path/to/Black Ops/main" "/path/to/Black Ops/localization.txt" user@server:blackops/
```

On a server without a desktop, install the system libraries the game needs (Ubuntu 24.04 / Debian):

```
sudo apt install libasound2t64 libpulse0 libsamplerate0 libxcursor1 libxi6 libxfixes3 libxrandr2 libxss1 libgbm1 libwayland-egl1 libwayland-cursor0 libdecor-0-0 libvulkan1 tmux
```

The server script checks them at start and tells you if one is missing.

## 2. Configure

Start the server once: it creates the settings file and stops.

```
./t5jool-server.sh
nano ~/.config/T5Jool/server.cfg
```

| Setting      | What it does                                                         |
|--------------|----------------------------------------------------------------------|
| `NAME`       | Name shown in the Online list                                        |
| `MAP`        | Map to run (`zombie_theater` = Kino der Toten, see the list in the file) |
| `MAXPLAYERS` | 1 to 4                                                               |
| `PORT`       | Game port (UDP). Port+1 is used for info and ping                    |
| `PUBLIC`     | `1` = shown in the Online list, `0` = private (LAN or direct connect) |
| `MOD`        | Community mod folder in `GAME_DIR/mods` (empty = base game). Players need the same mod |
| `GAME_DIR`   | Folder with `zone` and `main` (for example `/home/you/blackops`)     |

## 3. Open the ports

- **Home PC**: on your router, forward **UDP 3074** and **UDP 3075** to the PC's local address.
  Some providers use CGNAT: if the server never shows up online, ask your provider for a public IP.
- **Rented server (VPS)**: allow **UDP 3074-3075** in the provider's firewall panel. Many panels end with a
  "Drop all" rule: everything not listed is blocked, so keep **TCP 22** (SSH) in the list too or you lose access.
  Check that the range really includes **3074** (the game port), not only 3075. On the server:
  ```
  sudo ufw allow 3074:3075/udp
  ```

## 4. Check the ports

From **another computer** (not the server), replace `SERVER_IP`:

```
python3 -c "import socket;s=socket.socket(2,2);s.settimeout(3);s.sendto(b'\xff\xff\xff\xffgetchallenge 0 x 0',('SERVER_IP',3074));print(s.recv(1500)[:60])"
python3 -c "import socket;s=socket.socket(2,2);s.settimeout(3);s.sendto(b'T5JOOL_FIND 1',('SERVER_IP',3075));print(s.recv(500))"
```

Both must print an answer (`challengeResponse` and `T5JOOL_GAME`). A **timeout** means that port is blocked
by a firewall or the router. Run them while the server is running (step 5).

## 5. Start

```
./t5jool-server.sh
```

Stop it with **Ctrl+C**. The server restarts by itself after a crash, and an empty server restarts its match from round 1 after 60 seconds.
Logs are in `~/.local/share/T5Jool-server/logs/`.

### Keep it running after you log out

```
tmux new -s t5jool './t5jool-server.sh'
```

Detach with **Ctrl+B** then **D**: the server keeps running when you close the terminal.
Come back with `tmux attach -t t5jool`, stop it with `tmux kill-session -t t5jool`.

## More rooms on one server

One machine can run several servers ("rooms"), each with its own map. Give each room a name:

```
./t5jool-server.sh der-riese          # first time: creates ~/.config/T5Jool/server-der-riese.cfg and stops
nano ~/.config/T5Jool/server-der-riese.cfg
```

Set a different `NAME`, `MAP` and **`PORT`** in each room: 3074, 3084, 3094... Each room uses its port and the
next one (3084 and 3085), so open both in the firewall (for example UDP **3074-3105** for up to four rooms).
`GAME_DIR` is shared: all rooms use the same game files.

### Map rotation

To make a room change map after every match, list the maps in its config:

```
ROTATION="zombie_cod5_factory zombie_moon zombie_coast zombie_pentagon"
```

At the end of each match (or when the room stays empty for a minute) the server moves to the next map, and the
players' games reconnect to it by themselves after a few seconds. Leave `ROTATION=""` for a room
that always runs `MAP`.

Start every room in its own tmux session:

```
tmux new -d -s t5jool './t5jool-server.sh'
tmux new -d -s t5jool-der-riese './t5jool-server.sh der-riese'
tmux ls                                  # list the running rooms
```

Each room uses as much RAM as the first one: check with `free -h` before adding more.

## Update

Each release has its own folder (`T5Jool-v0.3-alpha-linux-x86_64`, ...). Your settings are in `~/.config/T5Jool/`,
so they stay the same. Download the new `.tar.gz`, then (replace the version with the new one):

```
V=0.3-alpha
tmux kill-server                          # stops every room
tar xf T5Jool-v$V-linux-x86_64.tar.gz
cd T5Jool-v$V-linux-x86_64
tmux new -d -s t5jool './t5jool-server.sh'
tmux new -d -s t5jool-der-riese './t5jool-server.sh der-riese'   # one line per room you have
```

Check with `tmux ls`, then delete the old version's folder and `.tar.gz`. Update the server when a new version is
released: players with a different version may not be able to join.

## Problems

- **Not in the Online list**: check that `PUBLIC=1`, the ports are open, and wait up to 30 seconds.
- **Players see the server but loading fails and they go back to the menu**: UDP 3074 is blocked, run the checks
  in step 4.
- **No ping (`-- ms`) in the list**: UDP 3075 is blocked.
- **Home internet with some providers (for example Sky WiFi in Italy, or mobile networks)**: the public IP may be shared (CGNAT /
  MAP-T) and port forwarding may not work. A cheap VPS is the easiest way around it.
- **The server keeps restarting**: start it with `T5_GDB=1 ./t5jool-server.sh` (needs `sudo apt install gdb`)
  and attach the output and the log to a bug report.
