<?php
header("Access-Control-Allow-Origin: *");
header("Content-Type: application/json; charset=UTF-8");
include 'conectare.php';
include 'verifica_admin.php';   // sesiune PHP (folosita pentru verificarea rolului de admin pe server)

$userPrimit  =isset($_GET['user'])  ? trim($_GET['user'])  : '';
$parolaPrimita = isset($_GET['pass'])  ? trim($_GET['pass'])  : '';
$tokenPrimit = isset($_GET['token']) ? trim($_GET['token']) : '';
$ipClient    = $_SERVER['REMOTE_ADDR'];

// Oprim imediat daca lipsesc date vitale
if (empty($userPrimit) || empty($parolaPrimita) || empty($tokenPrimit)) {
    echo json_encode(["status" => "error", "message" => "Date incomplete!"]);
    $conn->close();
    exit();
}

// Valori default pana citim din DB
$userGlobal  = "admin";
$parolaGlobala = "";

// Citim credentialele globale setate de superadmin din tabela setari
$rezultatSetari = $conn->query(
    "SELECT nume_setare, valoare FROM setari WHERE nume_setare IN ('global_user', 'global_pass')"
);
if ($rezultatSetari && $rezultatSetari->num_rows > 0) {
    while ($rand = $rezultatSetari->fetch_assoc()) {
        if ($rand['nume_setare'] == 'global_user') $userGlobal    = $rand['valoare'];
        if ($rand['nume_setare'] == 'global_pass') $parolaGlobala = $rand['valoare'];
    }
}

// Verificam parola — suporta atat parola default "1234" cat si parole hash-uite
$parolaCorecta = false;
if ($parolaGlobala === "" || $parolaGlobala === "1234") {
    if ($parolaPrimita === "1234") $parolaCorecta = true;
} else {
    if (password_verify($parolaPrimita, $parolaGlobala)) $parolaCorecta = true;
}

if ($userPrimit === $userGlobal && $parolaCorecta) {
    $rol = "vizitator";

    // Verificam daca device token-ul e deja inregistrat in sistem
    $verificareToken = $conn->prepare("SELECT rol_acces FROM utilizatori WHERE device_token = ?");
    $verificareToken->bind_param("s", $tokenPrimit);
    $verificareToken->execute();
    $rezultatToken = $verificareToken->get_result();

    if ($rezultatToken && $rezultatToken->num_rows > 0) {
        // Dispozitiv cunoscut: preluam rolul si actualizam datele + activ=1
        $rand = $rezultatToken->fetch_assoc();
        $rol  = $rand['rol_acces'];

        $actualizare = $conn->prepare(
            "UPDATE utilizatori SET ip_dispozitiv=?, ultima_logare=CURRENT_TIMESTAMP, activ=1 WHERE device_token=?"
        );
        $actualizare->bind_param("ss", $ipClient, $tokenPrimit);
        $actualizare->execute();
        $actualizare->close();
    } else {
        // Dispozitiv nou: il inregistram cu rol vizitator si activ=1
        $inserare = $conn->prepare(
            "INSERT INTO utilizatori (device_token, ip_dispozitiv, username, rol_acces, activ) VALUES (?, ?, ?, ?, 1)"
        );
        $numeImplicit = "Dispozitiv Nou";
        $rolImplicit  = "vizitator";
        $inserare->bind_param("ssss", $tokenPrimit, $ipClient, $numeImplicit, $rolImplicit);
        $inserare->execute();
        $inserare->close();
    }
    $verificareToken->close();

    // Creem sesiunea PHP a acestui browser. Retine doar device_token-ul; rolul se citeste din DB
    // la fiecare cerere protejata (vezi verifica_admin.php), deci nu poate fi falsificat din browser.
    porneste_sesiune();
    session_regenerate_id(true);
    $_SESSION['token'] = $tokenPrimit;

    echo json_encode(["status" => "success", "rol" => $rol, "ip" => $ipClient, "user" => $userPrimit]);
} else {
    echo json_encode(["status" => "error", "message" => "Autentificare esuata!"]);
}

$conn->close();
?>