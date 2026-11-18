<?php
/**
 * gestiune_utilizatori.php
 * Permite administratorului sa vizualizeze, modifice si stearga utilizatorii inregistrati.
 * Fiecare utilizator este identificat prin ip_dispozitiv si device_token.
 *
 * Actiuni disponibile:
 *   ?action=get             => returneaza toti utilizatorii ordonati dupa ultima logare
 *   ?action=update  (POST)  => actualizeaza rol si/sau nume pentru unul sau mai multi utilizatori
 *   ?action=delete&ip=X     => sterge utilizatorul cu ip_dispozitiv = X
 *
 * Raspuns get:    [ { "ip_dispozitiv": "...", "username": "...", "rol_acces": "...", ... }, ... ]
 * Raspuns update: { "status": "success" }
 * Raspuns delete: { "status": "success" }
 */

header("Access-Control-Allow-Origin: *");
header("Content-Type: application/json; charset=UTF-8");
include 'conectare.php';
include 'verifica_admin.php';

// Toate actiunile de aici (vizualizare, modificare rol, stergere) sunt doar pentru admin.
// Verificarea se face pe server, pe baza sesiunii create la login (nu pe rolul din browser).
cere_admin($conn);

$actiune = isset($_GET['action']) ? $_GET['action'] : '';

if ($actiune === 'get') {
    $rezultat = $conn->query("SELECT * FROM utilizatori ORDER BY ultima_logare DESC");
    $utilizatori = [];
    if ($rezultat && $rezultat->num_rows > 0) {
        while ($rand = $rezultat->fetch_assoc()) {
            $utilizatori[] = $rand;
        }
    }
    echo json_encode($utilizatori);
}
elseif ($actiune === 'update') {
    $date = json_decode(file_get_contents("php://input"), true);
    if ($date) {
        // Pregatim ambele variante de UPDATE (cu sau fara modificare de nume)
        $stmtComplet = $conn->prepare("UPDATE utilizatori SET rol_acces=?, username=? WHERE ip_dispozitiv=?");
        $stmtDoarRol = $conn->prepare("UPDATE utilizatori SET rol_acces=? WHERE ip_dispozitiv=?");
        foreach ($date as $ip => $valori) {
            if (is_array($valori)) {
                $stmtComplet->bind_param("sss", $valori['rol'], $valori['nume'], $ip);
                $stmtComplet->execute();
            } else {
                $stmtDoarRol->bind_param("ss", $valori, $ip);
                $stmtDoarRol->execute();
            }
        }
        if ($stmtComplet) $stmtComplet->close();
        if ($stmtDoarRol) $stmtDoarRol->close();
        echo json_encode(["status" => "success"]);
    } else {
        echo json_encode(["status" => "error", "mesaj" => "Nu s-au primit date."]);
    }
}
elseif ($actiune === 'delete') {
    $ip = isset($_GET['ip']) ? $_GET['ip'] : '';
    if ($ip !== '') {
        $stergere = $conn->prepare("DELETE FROM utilizatori WHERE ip_dispozitiv=?");
        $stergere->bind_param("s", $ip);
        if ($stergere->execute()) {
            echo json_encode(["status" => "success"]);
        } else {
            echo json_encode(["status" => "error", "mesaj" => "Eroare la stergere."]);
        }
        $stergere->close();
    } else {
        echo json_encode(["status" => "error", "mesaj" => "IP invalid."]);
    }
}
$conn->close();
?>