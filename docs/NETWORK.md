# Network

What UnleashX does on the network in the background. Written from the
network thread (`src/app/NetworkThread.cpp`) and `src/net/Network.cpp`. The
FTP server has its own page, [FTP.md](FTP.md).

## Settings

```xml
<Settings>
  <Network Enable="Yes">
    <DHCP>Yes</DHCP>
    <IP>192.168.0.5</IP>
    <Subnet>255.255.255.0</Subnet>
    <Gateway>192.168.0.1</Gateway>
    <DNS1>192.168.0.1</DNS1>
    <DNS2>192.168.0.7</DNS2>
    <AutoDetect>Yes</AutoDetect>
  </Network>
  <Newsfeed Enable="False" Interval="30">
    <URL>www.xbox-scene.com/xbox1data/xbox-scene.xml</URL>
  </Newsfeed>
  <Preference>
    <SNTP Synchronize="Yes" Port="123">192.168.0.1</SNTP>
    <LiveUpdate AutoCheck="Yes" EnableWIP="No">http://example.com/unleashx/</LiveUpdate>
  </Preference>
</Settings>
```

The SNTP server has to be an IP address (other values are ignored).

## The network thread

Started at boot (and by the `netreset` action and `SITE NETRESET`) when
`<Network Enable>` is on.

* While the Ethernet cable is unplugged it checks again every 2 seconds.
* When the link comes up it writes the `<Network>` settings to the Xbox's
  network configuration (if they changed), starts XNet and waits for an
  address. The settings strings are then filled with the configuration in
  use, so the DHCP results show in the settings screens. The thread then
  drops to below-normal priority.
* Every 2.5 seconds after that (unless a boost FTP transfer has paused
  everything):
  * The FTP server is started if `<FTP Enable>` is on and it isn't running,
    with the current `<FTP>` settings.
  * Unless an automatic launch is pending:
    * Once, 30 seconds after boot, when `<LiveUpdate AutoCheck>` is on and
      UnleashX isn't running from DVD: the live update check (below).
    * Otherwise, once the news ticker has started and `<Newsfeed Enable>` is
      on, one newsfeed per minute is downloaded (below).
    * Once, when `<SNTP Synchronize>` is on: the clock is set from the SNTP
      server.

## Newsfeeds

The config loader adds the first `<URL>` at startup; the network thread
adds the others the first time it gets to the newsfeeds. Then each minute
the next URL is fetched over HTTP (port 80), handed to the news ticker and
saved as `<data>\Newsfeed\<file name>`.
After the last feed the next round waits until the Xbox has been on for 5
more minutes than the previous round (30 more after the first hour).

## Live update

UnleashX fetches `Version.xml` from the `<LiveUpdate>` URL, by default
`http://ux.home.insightbb.com/` (spelled out character by character in the
code so it doesn't appear as a string). The file looks like:

```xml
<Release Major="0" Minor="39" Revision="584" Build="528" Type="A" MD5Hash="..." />
<WIP Major="0" Minor="40" Revision="600" Build="101" Type="" MD5Hash="..." />
```

If the `Release` entry (or, with `EnableWIP="Yes"`, the `WIP` entry) has an
`MD5Hash` and isn't older than this build, the dashboard shows "UnleashX
V0.40.0101 Build 600 is available for download." Quirk: "older" means
version 0.x with a minor version below 40 and a revision below 585, so the
check doesn't compare properly with other versions.

## Discovery

`ScanNetworkClients` broadcasts `ping` on UDP 4905 and lists the Xboxes that
answer; the FTP server answers the same ping (see FTP.md).
