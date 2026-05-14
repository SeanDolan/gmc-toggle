<?php
declare(strict_types=1);

$dataDir = __DIR__ . '/data';
$device = $_GET['device'] ?? '';
$device = is_string($device) ? $device : '';
$stateOnly = isset($_GET['state']);

function plain_response(string $text): void
{
    header('Content-Type: text/plain; charset=utf-8');
    header('Cache-Control: no-store');
    echo $text;
}

function latest_state(string $dataDir): string
{
    $files = glob($dataDir . '/*.json') ?: [];
    $latestFile = null;
    $latestTime = 0;

    foreach ($files as $file) {
        $mtime = filemtime($file) ?: 0;
        if ($mtime > $latestTime) {
            $latestTime = $mtime;
            $latestFile = $file;
        }
    }

    if ($latestFile === null) {
        return 'UNKNOWN';
    }

    $payload = json_decode((string) file_get_contents($latestFile), true);
    return is_array($payload) && isset($payload['data']['value']) ? (string) $payload['data']['value'] : 'UNKNOWN';
}

if ($device !== '') {
    if (!preg_match('/^[A-Za-z0-9_]{1,20}$/', $device)) {
        http_response_code(400);
        plain_response('INVALID DEVICE');
        exit;
    }

    $file = $dataDir . '/' . $device . '.json';
    if (!is_file($file)) {
        plain_response('UNKNOWN');
        exit;
    }

    $payload = json_decode((string) file_get_contents($file), true);
    $state = is_array($payload) && isset($payload['data']['value']) ? (string) $payload['data']['value'] : 'UNKNOWN';

    if ($stateOnly) {
        plain_response($state);
        exit;
    }
} else {
    $state = latest_state($dataDir);

    if ($stateOnly) {
        plain_response($state);
        exit;
    }
}

?><!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>GMC Toggle State</title>
  <style>
    html, body {
      margin: 0;
      min-height: 100%;
      font-family: Arial, sans-serif;
      background: #ffffff;
      color: #111111;
    }

    body {
      display: grid;
      place-items: center;
    }

    #state {
      font-size: clamp(4rem, 18vw, 12rem);
      font-weight: 700;
      line-height: 1;
    }
  </style>
</head>
<body>
  <main id="state"><?= htmlspecialchars($state, ENT_QUOTES, 'UTF-8') ?></main>
  <script>
    async function refreshState() {
      const params = new URLSearchParams(window.location.search);
      params.set('state', '1');
      const response = await fetch(window.location.pathname + '?' + params.toString(), {
        cache: 'no-store',
        headers: { 'Accept': 'text/plain' }
      });

      if (response.ok) {
        document.getElementById('state').textContent = (await response.text()).trim() || 'UNKNOWN';
      }
    }

    setInterval(refreshState, 1000);
  </script>
</body>
</html>
