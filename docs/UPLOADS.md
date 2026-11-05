# Bring a drive home

Dash-Chan 0.8.1 beta lets you configure **WiGLE, WDGWars and Wardrift directly in `/dashchan/config.json` on the SD card**. No website, browser or USB configuration tool is required. Each destination can be enabled independently. The device can send one drive or all saved drives. Uploading shares recorded coordinates, timestamps, network names, addresses and signal metadata with the selected service. Watching and saving a drive require no account or internet connection.

## SD card setup: step by step

1. **Check the firmware version.** Use **0.8.1-beta or newer** for the filename `config.json`. Version 0.8.0 uses the same schema-2 contents but requires the old name `upload.json`. Earlier versions do not support Wardrift. If installing from M5Burner, check the selected version; a submitted update may still be awaiting review. Follow the [installation guide](FLASHING.md) or [M5Burner calibration/backup guide](M5BURNER.md) when updating.
2. **Gather your credentials.** Use your home **2.4 GHz Wi-Fi SSID and password**. For WiGLE, obtain the **API name and API token** from [your account](https://wigle.net/account), not your login password. For WDGWars, obtain the **64-character hexadecimal API key** from [your profile](https://wdgwars.pl/profile). For Wardrift, create a **device API key** in [your Wardrift account](https://wardrift.net). Only obtain credentials for services you plan to enable. Keep them out of chat, screenshots and public repositories.
3. **Stop and save any drive, wait for uploads to finish, then power off Dash-Chan.** Remove the microSD card and insert it into your computer. Do not format it. Existing logs and pet progress should stay on the card.
4. **Open the SD card's root folder and create `dashchan` if it does not already exist.** Inside it, create `config.json`. The complete device path must be `/dashchan/config.json`, not `/config.json` or `/dashchan/dashchan/config.json`.
5. **Copy the template below into a plain-text editor**, or download [config.example.json](../sdcard/dashchan/config.example.json) and rename it to `config.json`. Enter your Wi-Fi credentials. Change a destination from `false` to `true` and fill its credential fields to enable it. **Enable at least one destination**; the blank template is deliberately unconfigured. You may enable all three. Unused credential strings can stay empty.
6. **Save as UTF-8 JSON.** On Windows, choose **Save as type: All files** and the exact filename `config.json`. Enable **View → Show → File name extensions** in File Explorer to ensure it is not `config.json.txt`. Keep the double quotes, commas, braces and unquoted `true`/`false`; JSON does not allow comments or trailing commas. If a password contains a double quote or backslash, escape it as `\"` or `\\`, or use the optional form below to generate the file safely.
7. **Safely eject the card, reinsert it, and power on Dash-Chan.** Open **Menu → Upload drives → Refresh**. The enabled destinations should appear. A missing/invalid-configuration message means you should check the filename, folder, JSON syntax and enabled service credentials.
8. **Test one saved drive while within range of your home Wi-Fi.** In **Menu → Upload drives**, select a drive with the arrows, tap **Send selected**, review the destination list, then tap again to confirm. Wait for the result and check that service's account. No networks are uploaded merely by saving the configuration or pressing Refresh.
9. **Send the backlog when ready.** Tap **Upload all (N)**, review the destination list, then **Confirm all (N)**. Confirmed destinations are skipped on later attempts; original CSV files remain on SD. See the receipt guidance below if an outcome is uncertain.
10. **Choose automatic behavior explicitly.** Set `automatic` to `true`, save the file and Refresh to attempt each new nonempty drive after **Stop + Save**. Keep it `false` for manual uploads only. Automatic mode tries at that moment: it does **not** wait for your arrival home, upload an old backlog at boot, or retry failed sends. If home Wi-Fi was unavailable when you stopped, use **Send selected** or **Upload all** when you get home.

```json
{
  "schema": 2,
  "ssid": "YOUR_2.4_GHZ_WIFI_NAME",
  "password": "YOUR_WIFI_PASSWORD",
  "wigle": false,
  "wigleName": "",
  "wigleToken": "",
  "wdg": false,
  "wdgKey": "",
  "wardrift": false,
  "wardriftToken": "",
  "automatic": false,
  "donate": false
}
```

| Setting | Enter or choose |
| --- | --- |
| `ssid`, `password` | Your 2.4 GHz home Wi-Fi name and password. An empty password is allowed for an open network. Enterprise login and captive portals are unsupported. |
| `wigle`, `wigleName`, `wigleToken` | Set `wigle` to `true` and enter the WiGLE API name and token. |
| `wdg`, `wdgKey` | Set `wdg` to `true` and enter the 64-character hexadecimal WDGWars API key. |
| `wardrift`, `wardriftToken` | Set `wardrift` to `true` and enter the Wardrift device API key. |
| `automatic` | `false` for manual control; `true` for a single attempt after each Stop + Save. |
| `donate` | Optional WiGLE commercial-use donation, off by default. This does not enable uploads by itself. |
| `schema` | Leave at `2`. Keep every field in the template, even for disabled destinations. |

### Existing upload.json files and safe removal

On **0.8.1+**, an existing `/dashchan/upload.json` still works. You can rename it to `/dashchan/config.json` without changing its contents. When both names exist, **config.json takes precedence**. An invalid `config.json` is reported as invalid; it does not silently fall back to the old account or automatic-upload choice. An interrupted new-file write can recover from `config-backup.json`; the legacy backup is used only when there is no new configuration or new backup.

To remove saved credentials and disable uploads, stop any upload and power off before editing the card. Remove **`config.json`, `config-new.json`, `config-backup.json`, `upload.json`, `upload-new.json`, and `upload-backup.json`** from `/dashchan/` if present, then reinsert/restart and Refresh. Removing only the main file can restore a recovery copy or load a legacy file. Leave your logs and `/dashchan/uploads/` receipts intact.

The configuration contains readable Wi-Fi passwords and API credentials. Keep the card, downloaded configuration and recovery copies private. The public example contains no real credentials.

### Troubleshooting the first upload

- **Missing or invalid config:** check `/dashchan/config.json`, the `.txt` extension, firmware 0.8.1+, valid JSON, and at least one enabled destination. Each enabled service needs its own correct API credentials.
- **Wi-Fi connection failed:** check 2.4 GHz coverage, exact SSID/password and that the network has internet access. A 5 GHz-only network or captive portal will not work. Retry manually after correcting it.
- **No drives found:** uploadable logs are `/wardrives/dash-########.csv`. Start a wardrive with usable GPS and SD storage, then Stop + Save. A blank log has no observations to send.
- **Authentication/rate-limit error:** verify the key at the named service and resolve its error before retrying. API access and service rules are controlled by that service.
- **Pending/uncertain result:** check the account first; do not repeatedly press Upload all or delete all receipts. Follow the receipts section below.

The device pauses scanning while uploading and resumes afterward. Live-account acceptance and long uploads remain beta validation items; a transport receipt is not proof of service-side processing or credit.

## Browser

1. Open [Upload drives](https://valleytech-dash-chan.pages.dev/#uploads).
2. Choose a CSV from `/wardrives/` on your SD card. Review Wi-Fi/BLE observation counts, coordinate bounds and the first five rows. Counts are observations, not unique devices.
3. For WiGLE, enter the **API name and API token** from [your account](https://wigle.net/account), not your login password. Check the sharing box and choose **Send to WiGLE**. Commercial-use donation is a separate choice, off by default.
4. For WDGWars, **Download WDGWars CSV** converts Dash-Chan's original 11-column WiGLE 1.4 output to the documented 14-column WiGLE 1.6 layout. Use the file on the [WDGWars profile upload page](https://wdgwars.pl/profile), or configure direct device upload below.

As checked on 2026-10-17, WiGLE permits the site's authenticated browser request; WDGWars rejects its cross-origin preflight. Dash-Chan does not bypass this with an opaque `no-cors` request or route your keys/logs through a third-party proxy. A browser or service policy change may affect direct uploads.

Files and credentials stay in browser memory unless you download them. There is no automatic browser upload or retry. An accepted response means the service accepted the file, not that all rows were processed or counted as new networks. Canceling or losing a connection can leave the outcome uncertain; inspect your service account before sending again.

The browser accepts the documented WiGLE 1.4/1.6 columns, at most 40 MiB and 500,000 rows. It rejects malformed quoting, unsupported multiline fields, missing/out-of-range coordinates, invalid timestamp/signal syntax, and files with no observations. Original files are never edited. Conversion fills known 2.4 GHz frequencies; BLE frequency is unknown (0), RCOIs are blank, and manufacturer ID is unknown (0).

## Optional website form or USB setup

If you prefer a form over editing JSON, use [Upload drives](https://valleytech-dash-chan.pages.dev/#uploads) → **Upload from StackChan**. This creates the same SD-card configuration:

1. Enter your **2.4 GHz** Wi-Fi name/password. Open networks are supported with an empty password. Enterprise authentication and captive portals are not supported.
2. Enable any combination of WiGLE, WDGWars and Wardrift. WiGLE uses its API name and token; WDGWars uses a 64-character hexadecimal API key; Wardrift uses a device API key from your Wardrift account. These are API credentials, not account passwords.
3. Leave automatic upload off for manual control, or explicitly enable **Automatically upload each drive after Stop + Save**.
4. Choose **Send setup over USB** with Dash-Chan running normally, an SD card inserted, recording stopped, and the installer disconnected. Desktop Chrome/Edge is required. The device validates the configuration and verifies the SD write before acknowledging it. Sending setup does not itself send a drive.
5. Alternatively, download `config.json`, copy it to `/dashchan/` on SD, and choose **Menu → Upload drives → Refresh**.

The file contains readable Wi-Fi passwords and API credentials. Keep it private, including any backup/download copy. Do not commit it, paste it into an issue, or bundle it with drive logs. Follow the safe-removal steps above to disable device uploads. Revoke API keys at the service if exposed. API access and service rules are controlled by those services.

## Send a saved drive

**Menu → Upload drives** shows your enabled destinations. Use the arrows to choose from the latest 24 files, then tap **Send selected** twice to confirm. Alternatively, tap **Upload all (N)**, then **Confirm all (N)** to process every `dash-########.csv` file in `/wardrives/`, including files older than the picker’s 24-file window. The service list and sharing notice remain visible. Successful per-destination receipts are skipped. Empty, malformed, unreadable or oversized files are counted as needing attention; valid files can still proceed. A service failure stops the batch after the current file, avoiding repeated bad-key or rate-limit requests. Other enabled services can still finish that file. Retry manually after resolving the problem; completed destinations will be skipped.

The robot pauses Wi-Fi/BLE watching and uses PSRAM for larger TLS buffers while leaving the BLE host initialized. It validates the CSV, creates a temporary compatible copy, joins the configured network, obtains internet time if needed, and streams the file over HTTPS using the ESP-IDF CA certificate bundle. It does not disable certificate verification or follow redirects with credentials. WiGLE receives the original 1.4 file; WDGWars receives a 1.6 copy; Wardrift receives raw 1.6 CSV in row-aligned chunks of at most 1 MiB. Every chunk repeats the CSV headers and has a stable filename and individual receipt. All rows are retained; no downsampling or original-file editing occurs. Recording is blocked during upload. A worker task keeps the face/settings responsive. **Cancel upload** may take up to the current 15-second network operation to finish.

On completion, failure or cancellation, the temporary copy is removed, network credentials are cleared from the Wi-Fi driver's RAM, and passive watching resumes. Original drives stay on SD. Do not remove the SD card during upload. A failed radio restart is reported and requires a restart.

Automatic mode only attempts the newly stopped, nonempty drive. It does not upload old files at boot, wait indefinitely for home Wi-Fi, or retry a failed request. If your network is unavailable, select the saved drive manually later. Network connection timeout is 25 seconds. Internet-time acquisition is bounded to 20 seconds. Device uploads accept original Dash-Chan 1.4 files up to 38 MiB; the converted copy must fit the 40 MiB limit. Allow space for the converted copy plus a 1 MiB Wardrift chunk. Device uploads remain a beta integration; a transport acceptance receipt does not guarantee service-side credit. Use browser/manual tools for larger files or externally edited layouts.

## Receipts and uncertain outcomes

`/dashchan/uploads/` contains receipts scoped to the original file SHA-256, service, and a fingerprint of the configured API identity. New names are `HASH-SERVICE-ACCOUNT-v1-PART.accepted` or `.pending`. Part 0 means the whole WiGLE/WDGWars file; Wardrift chunks start at 1. The fingerprint is not the key itself. Changing credentials makes a new destination identity; keep the original API key when resuming a batch. Legacy 0.5–0.7 receipts are still respected to avoid accidental repeat uploads, but are not account-scoped.

Receipt states:

- `.accepted`: the service confirmed acceptance. Repeating that exact file skips the service. This does not assert every row was imported or rewarded.
- `.pending`: sending was about to begin, but acceptance has not been durably recorded. Network loss, cancellation, power loss, an unrecognized response, or a receipt-write error can cause this. Subsequent attempts refuse to resubmit it automatically.

The receipt contains the original drive path, not credentials. The screen summarizes sent, skipped and attention counts; `/dashchan/uploads/last-result.txt` keeps the batch summary and last service error. For Wardrift, a count represents the drive’s destination as a whole; its chunk receipts retain partial progress. Check that file in the service account first. If it **was received**, retain the receipt; if it **was not received** and you intentionally want another attempt, remove only its matching `.pending` file from SD, then retry manually. Never clear the whole receipts directory as a routine troubleshooting step. Known HTTP 4xx rejection (except ambiguous 408/409) clears the pending marker, but does not trigger a retry. Respect service rate/size limits.

Interrupted USB setup writes use `config-new.json` and `config-backup.json` to preserve the previous configuration if renaming fails. These are also private credential files. A missing main configuration can be recovered from the backup on Refresh. The transfer buffer is bounded to 2 KiB and expires after ten seconds without a chunk; it never echoes secrets.

## Token file format

The web builder exports **schema 2**; the firmware still reads schema 1 (Wardrift disabled). USB setup checks the firmware schema before sending any settings, so an older build cannot silently ignore Wardrift. Firmware **0.8.1+** is required for the filename `config.json`; 0.8.0 requires `upload.json` instead. USB setup uses the filename supported by the running firmware.

```json
{
  "schema": 2,
  "ssid": "YOUR_2.4_GHZ_NETWORK",
  "password": "YOUR_WIFI_PASSWORD",
  "wigle": true,
  "wigleName": "YOUR_WIGLE_API_NAME",
  "wigleToken": "YOUR_WIGLE_API_TOKEN",
  "wdg": true,
  "wdgKey": "REPLACE_WITH_YOUR_64_HEX_CHARACTER_WDGWARS_KEY",
  "wardrift": true,
  "wardriftToken": "YOUR_WARDRIFT_DEVICE_API_KEY",
  "automatic": false,
  "donate": false
}
```

Replace the placeholders before saving; the WDGWars placeholder intentionally fails validation. Unused credential strings can be empty when their destination is false. At least one destination must be true. SSID/password limits are 32/64 UTF-8 bytes, WiGLE fields 128 visible ASCII characters each, Wardrift key 256 visible ASCII characters, and WDGWars exactly 64 hex characters. No control characters are accepted. The browser does not store these tokens in localStorage; downloads and SD copies are readable files.

## API references and test limits

Implemented against the [WiGLE API specification](https://api.wigle.net/swagger.json) and [WDGWars developer documentation](https://www.wdgwars.pl/press), and the [Wardrift direct-upload integration proposal](https://github.com/justcallmekoko/ESP32Marauder/pull/1634), reviewed 2026-11-04. Wardrift’s public site presented a browser challenge during this review, so its protocol was cross-checked against that published integration and remains subject to live-account acceptance. No service logos or firmware source were copied. WiGLE uses HTTP Basic API credentials; WDGWars uses `X-API-Key`; both use multipart field `file`. Wardrift uses `X-API-Key`, `X-File-Name`, and a raw `text/csv` body at `https://wardrift.net/v1/ingest/signals`. Only a complete HTTP 200 JSON response with the expected success boolean is accepted (Wardrift accepts `accepted: true` or `duplicate: true`). A 202 queued job is not reported as completed; check the service account and reconcile its pending receipt. Redirects, truncated JSON, HTML challenges and HTTP 409 are not success.

Automated tests execute the actual `src/uploads.cpp` worker with synthetic CSVs and simulated SD/HTTP/platform interfaces. Coverage includes all three request formats, fragmented response reads and partial writes, 31-file batches, credential changes, accepted/pending receipts, interrupted sends, HTTP 429, truncated responses, SD rename/write failure, cancellation, recording refusal, opt-in automation, and complete multi-chunk Wardrift row preservation. Parser checks also cover schema compatibility, duplicate JSON keys and response acceptance. Browser tests cover validation and USB refusal before mutation on old firmware. Hardware checks are documented in [Development](DEVELOPMENT.md). **Live account acceptance and long uploads remain unverified**; no user's real observations or credentials were sent as part of development tests.
