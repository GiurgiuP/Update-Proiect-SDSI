<?php
/**
 * sesiune_activa.php
 * Gestioneaza prezenta activa a utilizatorilor pentru modul DINAMIC al ventilatiei.
 * Foloseste coloana `activ` din tabela `utilizatori` ca sursa de adevar.
 * Identificarea se face prin device_token — unic per browser, persistent in localStorage.
 *
 * Actiuni disponibile:
 *   ?action=login&token=X   => seteaza activ=1 pentru tokenul X (la autentificare)
 *   ?action=logout&token=X  => seteaza activ=0 pentru tokenul X (la deconectare)
 *   ?action=count           => returneaza numarul de utilizatori cu activ=1 (pentru ESP32)
 *
 * Raspuns count: { "status": "success", "activi": N }
 */

header("Access-Control-Allow-Origin: *");
header("Content-Type: application/json; charset=UTF-8");
include 'conectare.php';
include 'verifica_admin.php';

$actiune = isset($_GET['action']) ? trim($_GET['action']) : '';

// --- LOGIN: marcheaza utilizatorul ca prezent activ ---
if ($actiune === 'login') {
    $token = isset($_GET['token']) ? trim($_GET['token']) : '';
    if ($token === '') {
        echo json_encode(["status" => "error", "mesaj" => "Token lipsa"]);
        $conn->close();
        exit();
    }
    $stmt = $conn->prepare("UPDATE utilizatori SET activ=1 WHERE device_token=?");
    $stmt->bind_param("s", $token);
    $stmt->execute();
    $stmt->close();
    echo json_encode(["status" => "success", "activ" => 1]);
}

// --- LOGOUT: marcheaza utilizatorul ca inactiv ---
elseif ($actiune === 'logout') {
    // Inchidem si sesiunea PHP a acestui browser (daca exista), ca sa nu mai poata folosi
    // functiile de admin dupa deconectare. Afecteaza doar sesiunea celui care face cererea.
    porneste_sesiune();
    $_SESSION = [];
    session_destroy();

    $token = isset($_GET['token']) ? trim($_GET['token']) : '';
    if ($token === '') {
        echo json_encode(["status" => "error", "mesaj" => "Token lipsa"]);
        $conn->close();
        exit();
    }
    $stmt = $conn->prepare("UPDATE utilizatori SET activ=0 WHERE device_token=?");
    $stmt->bind_param("s", $token);
    $stmt->execute();
    $stmt->close();
    echo json_encode(["status" => "success", "activ" => 0]);
}

// --- COUNT: returneaza numarul de utilizatori logati (citit de ESP32 pentru modul DINAMIC) ---
elseif ($actiune === 'count') {
    $rezultat = $conn->query("SELECT COUNT(*) as nr FROM utilizatori WHERE activ=1");
    $rand = $rezultat->fetch_assoc();
    echo json_encode(["status" => "success", "activi" => (int)$rand['nr']]);
}

else {
    echo json_encode(["status" => "error", "mesaj" => "Actiune necunoscuta"]);
}

$conn->close();
?>