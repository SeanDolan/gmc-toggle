<?php
declare(strict_types=1);

header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store');

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    http_response_code(405);
    echo json_encode(['ok' => false, 'error' => 'POST required']);
    exit;
}

$rawBody = (string) file_get_contents('php://input');
$payload = json_decode($rawBody, true);

if (!is_array($payload)) {
    http_response_code(400);
    echo json_encode(['ok' => false, 'error' => 'Invalid JSON']);
    exit;
}

$deviceName = isset($payload['deviceName']) ? (string) $payload['deviceName'] : '';
$reedState = isset($payload['reedState']) ? strtoupper((string) $payload['reedState']) : '';

if (!preg_match('/^[A-Za-z0-9_]{1,20}$/', $deviceName)) {
    http_response_code(400);
    echo json_encode(['ok' => false, 'error' => 'Invalid deviceName']);
    exit;
}

if ($reedState !== 'ON' && $reedState !== 'OFF') {
    http_response_code(400);
    echo json_encode(['ok' => false, 'error' => 'Invalid reedState']);
    exit;
}

$dataDir = __DIR__ . '/data';
if (!is_dir($dataDir) && !mkdir($dataDir, 0775, true)) {
    http_response_code(500);
    echo json_encode(['ok' => false, 'error' => 'Unable to create data directory']);
    exit;
}

$record = [
    'deviceName' => $deviceName,
    'reedState' => $reedState,
    'reedClosed' => isset($payload['reedClosed']) ? (bool) $payload['reedClosed'] : null,
    'receivedAt' => gmdate('c'),
    'sourceIp' => $_SERVER['REMOTE_ADDR'] ?? null,
    'payload' => $payload,
];

$file = $dataDir . '/' . $deviceName . '.json';
$encoded = json_encode($record, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES);

if ($encoded === false || file_put_contents($file, $encoded . PHP_EOL, LOCK_EX) === false) {
    http_response_code(500);
    echo json_encode(['ok' => false, 'error' => 'Unable to write state']);
    exit;
}

echo json_encode(['ok' => true, 'deviceName' => $deviceName, 'reedState' => $reedState]);
