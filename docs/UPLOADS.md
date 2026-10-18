# Bring a drive home

Dash-Chan 0.5 supports browser WiGLE uploads, a WDGWars-compatible export, and optional device uploads to both services. Uploading shares recorded coordinates, timestamps, network names, addresses and signal metadata with the selected service. Watching and saving a drive require no account or internet connection.

## Browser

1. Open [Upload drives](https://valleytechsolutions.github.io/Dash-Chan/#uploads).
2. Choose a CSV from `/wardrives/` on your SD card. Review Wi-Fi/BLE observation counts, coordinate bounds and the first five rows. Counts are observations, not unique devices.
3. For WiGLE, enter the **API name and API token** from [your account](https://wigle.net/account), not your login password. Check the sharing box and choose **Send to WiGLE**. Commercial-use donation is a separate choice, off by default.
4. For WDGWars, **Download WDGWars CSV** converts Dash-Chan's original 11-column WiGLE 1.4 output to the documented 14-column WiGLE 1.6 layout. Use the file on the [WDGWars profile upload page](https://wdgwars.pl/profile), or configure direct device upload below.

As checked on 2026-10-17, WiGLE permits the site's authenticated browser request; WDGWars rejects its cross-origin preflight. Dash-Chan does not bypass this with an opaque `no-cors` request or route your keys/logs through a third-party proxy. A browser or service policy change may affect direct uploads.

Files and credentials stay in browser memory unless you download them. There is no automatic browser upload or retry. An accepted response means the service accepted the file, not that all rows were processed or counted as new networks. Canceling or losing a connection can leave the outcome uncertain; inspect your service account before sending again.

The browser accepts the documented WiGLE 1.4/1.6 columns, at most 40 MiB and 500,000 rows. It rejects malformed quoting, unsupported multiline fields, missing/out-of-range coordinates, invalid timestamp/signal syntax, and files with no observations. Original files are never edited. Conversion fills known 2.4 GHz frequencies; BLE frequency is unknown (0), RCOIs are blank, and manufacturer ID is unknown (0).

## Configure StackChan

Use the same page's **Upload from StackChan** panel:

1. Enter your **2.4 GHz** Wi-Fi name/password. Open networks are supported with an empty password. Enterprise authentication and captive portals are not supported.
2. Enable WiGLE and/or WDGWars and enter the corresponding API credentials. WDGWars keys are 64 hexadecimal characters generated in your profile.
3. Leave automatic upload off for manual control, or explicitly enable **Automatically upload each drive after Stop + Save**.
4. Choose **Send setup over USB** with Dash-Chan running normally, an SD card inserted, recording stopped, and the installer disconnected. Desktop Chrome/Edge is required. The device validates the configuration and verifies the SD write before acknowledging it. Sending setup does not itself send a drive.
5. Alternatively, download `upload.json`, copy it to `/dashchan/` on SD, and choose **Menu → Upload drives → Refresh**.

The file contains readable Wi-Fi passwords and API credentials. Keep it private, including any backup/download copy. Do not commit it, paste it into an issue, or bundle it with drive logs. Delete `/dashchan/upload.json` and any interrupted-transfer `/dashchan/upload-backup.json`, then Refresh, to disable device uploads. Revoke API keys at the service if exposed. API access and service rules are controlled by those services.

## Send a saved drive

**Menu → Upload drives** shows the latest 24 `dash-########.csv` files and enabled destinations. Use the arrows, tap **Upload selected drive**, then **Confirm send to services**. This shares the entire selected drive. The service list and sharing notice remain visible before confirmation.

The robot pauses Wi-Fi/BLE watching and uses PSRAM for larger TLS buffers while leaving the BLE host initialized. It validates the CSV, creates a temporary compatible copy, joins the configured network, obtains internet time if needed, and streams the file over HTTPS using the ESP-IDF CA certificate bundle. It does not disable certificate verification or follow redirects with credentials. WiGLE receives the original 1.4 file; WDGWars receives the 1.6 copy. Recording is blocked during upload. A worker task keeps the face/settings responsive. **Cancel upload** may take up to the current 15-second network operation to finish.

On completion, failure or cancellation, the temporary copy is removed, network credentials are cleared from the Wi-Fi driver's RAM, and passive watching resumes. Original drives stay on SD. Do not remove the SD card during upload. A failed radio restart is reported and requires a restart.

Automatic mode only attempts the newly stopped, nonempty drive. It does not upload old files at boot, wait indefinitely for home Wi-Fi, or retry a failed request. If your network is unavailable, select the saved drive manually later. Network connection timeout is 25 seconds. Internet-time acquisition is bounded to 20 seconds. Device uploads accept original Dash-Chan 1.4 files up to 38 MiB; the converted copy must fit the 40 MiB limit. A free temporary-copy-sized amount of SD space is required. Use browser/manual tools for larger files or externally edited layouts.

## Receipts and uncertain outcomes

`/dashchan/uploads/` contains per-file-SHA-256, per-service receipts:

- `HASH-wigle.accepted` / `HASH-wdg.accepted`: the service confirmed acceptance. Repeating that exact file skips the service. This does not assert every row was imported or rewarded.
- `HASH-SERVICE.pending`: sending was about to begin, but acceptance has not been durably recorded. Network loss, cancellation, power loss, an unrecognized response, or a receipt-write error can cause this. Subsequent attempts refuse to resubmit it automatically.

The receipt contains the original drive path, not credentials. Check that file in the service account first. If it **was received**, retain the receipt; if it **was not received** and you intentionally want another attempt, remove only its matching `.pending` file from SD, then retry manually. Never clear the whole receipts directory as a routine troubleshooting step. Known HTTP 4xx rejection (except ambiguous 408) clears the pending marker, but does not trigger a retry. Respect service rate/size limits.

Interrupted USB setup writes use `upload-new.json` and `upload-backup.json` to preserve the previous configuration if renaming fails. These are also private credential files. A missing main configuration can be recovered from the backup on Refresh. The transfer buffer is bounded to 2 KiB and expires after ten seconds without a chunk; it never echoes secrets.

## API references and test limits

Implemented against the [WiGLE API specification](https://api.wigle.net/swagger.json) and [WDGWars developer documentation](https://www.wdgwars.pl/press), reviewed 2026-10-17. No service logos or firmware source were copied. WiGLE uses HTTP Basic API credentials; WDGWars uses `X-API-Key`; both use multipart field `file`.

Automated tests use synthetic CSVs and mocked service responses, including rejection, cancellation/error handling, CSV conversion and USB setup refusal. Hardware checks are documented in [Development](DEVELOPMENT.md). **Live account acceptance and long uploads remain unverified**; no user's real observations or credentials were sent as part of development tests.
