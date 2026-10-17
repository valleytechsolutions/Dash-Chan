# Evidence and coverage

The shipped adapter uses the passive parsing headers from Dragon Detector commit `3204e7a0caa3b107e751e929ec63ec1f80739828`. It does not import Dragon Detector's active radio tools. It supplements normal classification with its Axon evidence combiner. The UI retains the classifier's explanation and confidence.

| Family shown | Evidence and important limit |
|---|---|
| Axon / body camera | Registered company/service/OUI, names and supported payload clues; exact model and recording state unknown |
| Flock | Name, service, manufacturer combinations and curated address evidence; broad radio supplier prefixes remain weak |
| ALPR vendor | Vendor address hints; a vendor makes other products too |
| Camera glasses | Meta/Luxottica/Snap/Vuzix indicators; no claim that recording is active |
| Find My / tracker | Apple Find My, DULT, Samsung, Tile and Find Hub; these share one UI category, not a guaranteed AirTag model |
| Flipper Zero | Advertised service/name signatures; not the broad Flipper company ID alone |
| Biscuit family | Advertised name plus service; Pro/Ultra cannot be distinguished passively |
| Pineapple / Pwnagotchi | Names, curated address clues or peer beacon fields; configurable/spoofable |
| Nyanbox / HaleHound | Advertised names; weak clues |
| Serial / skimmer | Generic serial-module clues; most such modules may be benign |
| Camera / Ring vendor | Vendor evidence only |
| Raven candidate | Community service/vendor heuristics |
| Remote ID drone | BLE or beacon payload parsing via OpenDroneID; self-reported broadcasts |

This initial scanner receives **Wi-Fi beacons/probe responses and BLE advertisements**. It does not capture client probes, deauthentication traffic, NAN action frames, Bluetooth Classic, or all Dragon Detector traffic-analysis features. Deauth-burst, evil-twin, pairing-spam, mesh, and hunt-mode parity are not claimed. There are no active connections, pairing, APs, probe requests, deauth, jamming, or packet injection in this application.

Default alerts require medium evidence. Lowering the filter admits more ambiguous names and shared hardware vendors. All signatures can be spoofed; high confidence means a stronger signature match, not authenticated identity. Recognition depends on hardware advertising or beaconing within a scheduled receive window. There is no guarantee that all nearby equipment is detected.

Each encounter has a 60-second alert cooldown, with at least nine seconds between ordinary alerts. Contacts expire after two minutes unseen and occupy a bounded 256-entry table; dense areas can evict older entries. The last 20 alerted events are kept in RAM (the five newest are browsable on screen); event CSV contains actual emitted alerts while SD is available. Raw received advertisements are not persisted.

No real-world device captures are included in the repository. Tests use synthetic frames. Field comparison against actual Axon, ALPR, glasses, tags, Flipper, and Biscuit hardware is still required.
