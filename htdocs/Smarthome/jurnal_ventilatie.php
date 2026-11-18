<?php
/**
 * jurnal_ventilatie.php
 * Gestioneaza jurnalul de activitate al sistemului de ventilatie.
 * Pastreaza ultimele 10 actiuni (schimbari de mod, prag, pornire/oprire).
 *
 * Actiuni disponibile:
 *   ?action=get                                    => returneaza ultimele 10 intrari
 *   ?action=add&ip=X&temp=Y&mod=Z&prag=W           => adauga o intrare noua
 *
 * Raspuns get: [ { "data_ora": "...", "ip_utilizator": "...", "temperatura": "...", ... }, ... ]
 * Raspuns add: { "status": "success" }
 */

header("Access-Control-Allow-Origin: *");
header("Content-Type: application/json; charset=UTF-8");
include 'conectare.php';

$actiune = isset($_GET['action']) ? $_GET['action'] : '';

// --- GET: citeste ultimele 10 inregistrari din jurnal ---
if ($actiune === 'get') {
    $rezultat = $conn->query(
        "SELECT * FROM log_ventilatie ORDER BY data_ora DESC LIMIT 10"
    );
    $jurnalEntries = [];
    if ($rezultat && $rezultat->num_rows > 0) {
        while ($rand = $rezultat->fetch_assoc()) {
            $jurnalEntries[] = $rand;
        }
    }
    echo json_encode($jurnalEntries);
}

// --- ADD: adauga o intrare noua in jurnal si curata inregistrarile mai vechi ---
elseif ($actiune === 'add') {
    $ipUtilizator = isset($_GET['ip'])   ? trim($_GET['ip'])   : 'Necunoscut';
    $temperatura  = isset($_GET['temp']) ? trim($_GET['temp']) : '--';
    $modVentilatie = isset($_GET['mod']) ? trim($_GET['mod'])  : '--';
    $pragTemp     = isset($_GET['prag']) ? trim($_GET['prag']) : '--';

    $inserare = $conn->prepare(
        "INSERT INTO log_ventilatie (ip_utilizator, temperatura, mod_vent, prag) VALUES (?, ?, ?, ?)"
    );

    if ($inserare) {
        $inserare->bind_param("ssss", $ipUtilizator, $temperatura, $modVentilatie, $pragTemp);

        if ($inserare->execute()) {
            // Pastram doar ultimele 10 inregistrari pentru a nu umple baza de date
            $conn->query(
                "DELETE FROM log_ventilatie WHERE id NOT IN 
                (SELECT id FROM (SELECT id FROM log_ventilatie ORDER BY id DESC LIMIT 10) as t)"
            );
            echo json_encode(["status" => "success"]);
        } else {
            echo json_encode(["status" => "error", "message" => "Eroare la inserare."]);
        }
        $inserare->close();
    } else {
        echo json_encode(["status" => "error", "message" => "Eroare pregatire SQL."]);
    }
}

$conn->close();
?>