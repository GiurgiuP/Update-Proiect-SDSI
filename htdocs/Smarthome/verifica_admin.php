<?php
/**
 * verifica_admin.php
 * Functii comune pentru sesiunea PHP si verificarea rolului de admin, pe SERVER.
 *
 * De ce exista: rolurile din site.html/script.js sunt doar vizuale (se pot modifica din
 * consola browserului). Orice actiune care trebuie sa fie "doar admin" trebuie verificata
 * aici, pe server, pe baza sesiunii create la login.
 *
 * Cum functioneaza:
 *   1. login.php cheama porneste_sesiune() si salveaza device_token-ul in $_SESSION.
 *   2. Fisierele protejate cheama cere_admin($conn). Rolul NU este tinut in sesiune, ci este
 *      citit din baza de date la fiecare cerere, deci o retrogradare sau o stergere din
 *      "Dispozitive Conectate" se aplica imediat.
 *   3. Daca nu esti admin => HTTP 403 si raspuns JSON, scriptul se opreste.
 *
 * Fisiere care il folosesc: login.php, sesiune_activa.php, gestiune_utilizatori.php,
 * diag.php, jurnal_comenzi.php
 */

/** Durata sesiunii: 7 zile (ca sa nu expire cat timp site-ul tine login-ul in localStorage). */
define('DURATA_SESIUNE', 604800);

/**
 * Porneste sesiunea PHP o singura data, cu cookie HttpOnly (nu poate fi citit din JavaScript).
 * Fisierele de sesiune sunt tinute intr-un folder propriu, blocat la acces din browser.
 */
function porneste_sesiune() {
    if (session_status() !== PHP_SESSION_NONE) return;

    $folder = __DIR__ . '/sesiuni_php';
    if (!is_dir($folder)) {
        @mkdir($folder, 0700, true);
        @file_put_contents($folder . '/.htaccess', "Require all denied\n");
    }
    if (is_dir($folder) && is_writable($folder)) {
        session_save_path($folder);
    }

    ini_set('session.gc_maxlifetime', (string) DURATA_SESIUNE);
    session_set_cookie_params([
        'lifetime' => DURATA_SESIUNE,
        'path'     => '/',
        'httponly' => true,
        'samesite' => 'Lax'
    ]);
    session_start();
}

/**
 * Returneaza rolul curent din baza de date pentru sesiunea curenta, sau null daca nu exista
 * sesiune valida / dispozitivul nu mai exista in tabela utilizatori.
 */
function rol_din_sesiune($conn) {
    porneste_sesiune();
    if (empty($_SESSION['token'])) return null;

    $stmt = $conn->prepare("SELECT rol_acces FROM utilizatori WHERE device_token = ?");
    if (!$stmt) return null;
    $stmt->bind_param("s", $_SESSION['token']);
    $stmt->execute();
    $rezultat = $stmt->get_result();
    $rol = null;
    if ($rezultat && $rezultat->num_rows > 0) {
        $rand = $rezultat->fetch_assoc();
        $rol  = $rand['rol_acces'];
    }
    $stmt->close();
    return $rol;
}

/**
 * Opreste scriptul cu 403 daca cel care a facut cererea nu este admin. Returneaza rolul ("admin").
 */
function cere_admin($conn) {
    $rol = rol_din_sesiune($conn);
    if ($rol !== 'admin') {
        http_response_code(403);
        header("Content-Type: application/json; charset=UTF-8");
        echo json_encode([
            "status" => "error",
            "mesaj"  => "Acces interzis: sesiune expirata sau nu ai rol de admin."
        ]);
        $conn->close();
        exit();
    }
    return $rol;
}
?>
