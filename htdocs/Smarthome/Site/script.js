
// =============================================================================
// 1. CONFIGURARE SI VARIABILE GLOBALE
// IP-ul ESP32 si originea XAMPP sunt definite aici.
// Modifica ESP_IP daca schimbi adresa placii in retea.
// =============================================================================
const ESP_IP = "http://10.176.199.36"; 
const XAMPP_IP = window.location.origin; 

let paginaCurenta = "dashboard";
let loopDate = null; 
let totiUtilizatoriiDB = [];
let modificariNesalvate = {};
let modVentilatieCurent = ""; 

// =============================================================================
// 2. UTILITARE (TIMP, TOKEN, HELPERE)
// =============================================================================
/**
 * getDeviceToken()
 * Genereaza sau citeste un token unic pentru dispozitivul curent.
 * Tokenul este salvat in localStorage si persista intre sesiuni.
 * Folosit pentru identificarea dispozitivului in baza de date (tabela utilizatori).
 * @returns {string} Token unic de forma "DEV-xxxxxxx"
 */
function getDeviceToken() {
    let token = localStorage.getItem("deviceToken");
    if (!token) {
        token = "DEV-" + Math.random().toString(36).substr(2, 9) + Date.now().toString(36);
        localStorage.setItem("deviceToken", token);
    }
    return token;
}

/**
 * formateazaTimp(valoare)
 * Converteste un numar de secunde sau un timestamp in text lizibil.
 * Exemplu: 45 => "Acum 45 secunde", 3700 => "Acum 1 ora si 1 min"
 * @param {number|string} valoare - Secunde scurse sau data/ora ca string
 * @returns {string} Text formatat in romana
 */
function formateazaTimp(valoare) {
    if (!valoare || valoare === "Nedefinit") return "Nedefinit";
    let secundeTotale;
    if (typeof valoare === 'number' || !isNaN(valoare)) {
        secundeTotale = parseInt(valoare);
    } else {
        let dataLogare = new Date(valoare.replace(' ', 'T'));
        secundeTotale = Math.floor((new Date() - dataLogare) / 1000);
    }
    if (secundeTotale < 0) secundeTotale = 0;
    if (secundeTotale < 2) return "🟢 Chiar acum";
    if (secundeTotale < 60) return `Acum ${secundeTotale} secunde`;
    if (secundeTotale < 3600) return `Acum ${Math.floor(secundeTotale / 60)} min`;
    if (secundeTotale < 86400) {
        let h = Math.floor(secundeTotale / 3600);
        let m = Math.floor((secundeTotale % 3600) / 60);
        return `Acum ${h} ${h === 1 ? 'oră' : 'ore'} și ${m} min`;
    }
    let d = (typeof valoare === 'number' || !isNaN(valoare)) ? new Date(Date.now() - valoare * 1000) : new Date(valoare.replace(' ', 'T'));
    let zi = String(d.getDate()).padStart(2,'0');
    let luna = String(d.getMonth()+1).padStart(2,'0');
    let an = d.getFullYear();
    return `în ziua ${zi}.${luna}.${an}`;
}

/**
 * toggleVisibilityParola()
 * Comuta vizibilitatea parolei in campul de login (text/password).
 * Schimba si iconita ochiului corespunzator.
 */
function toggleVisibilityParola() {
    const inputParola = document.getElementById("input-pass");
    const iconOchi = document.getElementById("ochi-parola");
    const ochiInchisSVG = `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M17.94 17.94A10.07 10.07 0 0 1 12 20c-7 0-11-8-11-8a18.45 18.45 0 0 1 5.06-5.94M9.9 4.24A9.12 9.12 0 0 1 12 4c7 0 11 8 11 8a18.5 18.5 0 0 1-2.16 3.19m-6.72-1.07a3 3 0 1 1-4.24-4.24"></path><line x1="1" y1="1" x2="23" y2="23"></line></svg>`;
    const ochiDeschisSVG = `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"></path><circle cx="12" cy="12" r="3"></circle></svg>`;
    if (inputParola.type === "password") { inputParola.type = "text"; iconOchi.innerHTML = ochiInchisSVG; } 
    else { inputParola.type = "password"; iconOchi.innerHTML = ochiDeschisSVG; }
}

// =============================================================================
// 3. LOGICA DE AUTENTIFICARE ȘI SESIUNE
// =============================================================================
/**
 * verificaLogin()
 * Trimite credentialele la login.php si proceseaza raspunsul.
 * La succes: salveaza sesiunea in localStorage, incarca aplicatia.
 * La eroare: afiseaza mesaj si animatie de scuturare.
 */
async function verificaLogin() {
    let u = document.getElementById("input-user").value; 
    let p = document.getElementById("input-pass").value;
    const loginBox = document.querySelector('.login-box');
    const inputUser = document.getElementById('input-user');
    const inputPass = document.getElementById('input-pass');
    const mesajEroare = document.getElementById("mesaj-eroare");
    const btnLogin = document.querySelector('.btn-login');

    if(btnLogin) btnLogin.classList.add('btn-loading');
    if(mesajEroare) mesajEroare.innerText = "";

    try {
        let dToken = getDeviceToken();
        const raspuns = await fetch(`${XAMPP_IP}/smarthome/login.php?user=${u}&pass=${p}&token=${dToken}`);
        const date = await raspuns.json();

        if (date.status === "success") {
            window.rolUtilizator = date.rol; 
            window.ipUtilizator = date.ip; 
            window.numeUtilizator = date.user || u || "Vizitator";
            localStorage.setItem("rolUtilizator", date.rol);
            localStorage.setItem("ipUtilizator", date.ip);
            localStorage.setItem("numeUtilizator", window.numeUtilizator);

            if(btnLogin) btnLogin.classList.remove('btn-loading');
            const ecranLogin = document.getElementById("ecran-login");
            const ecranApp = document.getElementById("ecran-aplicatie");
            if(ecranLogin) ecranLogin.classList.add('fade-out-login');

            setTimeout(async () => {
                if(ecranLogin) ecranLogin.style.display = "none";
                if(ecranApp) ecranApp.style.display = "block";
                if(ecranApp) ecranApp.classList.add('fade-in-app');
                
                let titlu = document.querySelector(".titlu-pagina");
                if(titlu) titlu.innerText = `Smart Home - ${date.rol.toUpperCase()}`;
                aplicaPermisiuni();

                try {
                    const setariRes = await fetch(`${XAMPP_IP}/smarthome/get_settings.php`);
                    const setariDB = await setariRes.json();
                    if (setariDB.oras_vreme) { aduceVremea(setariDB.oras_vreme); } 
                    else { aduceVremea("Timișoara"); }
                } catch(e) { aduceVremea("Timișoara"); }

                schimbaPagina('dashboard');
                updatePeriodic(); 
                loopDate = setInterval(updatePeriodic, 2000);
                setezaSesiuneActiva(true); // confirma activ=1 in DB
            }, 600);
        } else {
            if(btnLogin) btnLogin.classList.remove('btn-loading');
            if(mesajEroare) mesajEroare.innerText = "User sau parolă incorecte!";
            if(loginBox) loginBox.classList.add('shake-error');
            if(inputUser) inputUser.classList.add('input-eroare');
            if(inputPass) inputPass.classList.add('input-eroare');
            setTimeout(() => {
                if(loginBox) loginBox.classList.remove('shake-error');
                if(inputUser) inputUser.classList.remove('input-eroare');
                if(inputPass) inputPass.classList.remove('input-eroare');
            }, 600);
        }
    } catch (error) { 
        if(btnLogin) btnLogin.classList.remove('btn-loading');
        if(mesajEroare) mesajEroare.innerText = "Eroare de comunicare cu Serverul!"; 
        if(loginBox) loginBox.classList.add('shake-error');
        if(inputUser) inputUser.classList.add('input-eroare');
        if(inputPass) inputPass.classList.add('input-eroare');
        setTimeout(() => {
            if(loginBox) loginBox.classList.remove('shake-error');
            if(inputUser) inputUser.classList.remove('input-eroare');
            if(inputPass) inputPass.classList.remove('input-eroare');
        }, 600);
    }
}

/**
 * delogare()
 * Curata sesiunea locala si seteaza activ=0 in baza de date.
 * Afiseaza animatia de iesire si revine la ecranul de login.
 */
function delogare() {
    clearInterval(loopDate);

    // Setam activ=0 in DB - simplu si sigur, nu depinde de ESP
    setezaSesiuneActiva(false);

    localStorage.removeItem("rolUtilizator");
    localStorage.removeItem("ipUtilizator");
    localStorage.removeItem("numeUtilizator");
    document.getElementById("input-user").value = "";
    document.getElementById("input-pass").value = "";
    document.getElementById("sidebar").classList.remove("deschis");

    let paznic = document.getElementById("paznic-login");
    if (paznic) paznic.remove();
    let elApp = document.getElementById("ecran-aplicatie");
    let elLogin = document.getElementById("ecran-login");

    elApp.classList.remove('fade-in-app');
    elApp.classList.add('fade-out-app');

    setTimeout(() => {
        elApp.style.display = "none";
        elLogin.classList.remove('fade-out-login');
        elLogin.style.display = "flex";
        elLogin.classList.add('fade-in-login');
        elApp.classList.remove('fade-out-app');
    }, 400);
}

// =============================================================================
// 4. UI & PERMISIUNI (CONTROL VIZUAL)
// =============================================================================
/**
 * aplicaPermisiuni()
 * Ascunde sau afiseaza elementele de interfata in functie de rolul utilizatorului.
 * Roluri: vizitator (doar vizualizare), proprietar (control), admin (acces complet).
 */
function aplicaPermisiuni() {
    let poateModifica = (window.rolUtilizator === "proprietar" || window.rolUtilizator === "admin");
    let esteAdmin = (window.rolUtilizator === "admin");

    const elementeRestrictive = [
        "grup-vreme-actiune", "btn-comuta-vent", "grup-prag-actiune",
        "btn-releu-lumina", "btn-soft-lumina", "grup-culoare-actiune",
        "grup-intensitate-actiune", "card-comanda-usa", "grup-putere-actiune" ,
        "grup-jurnal-vent" 
    ];
    elementeRestrictive.forEach(id => {
        let el = document.getElementById(id);
        if (el) el.style.display = poateModifica ? "" : "none"; 
    });

    let inputVreme = document.getElementById("input-oras-vreme");
    if (inputVreme) {
        inputVreme.disabled = !poateModifica; 
        inputVreme.style.opacity = poateModifica ? "1" : "0.5"; 
        inputVreme.placeholder = poateModifica ? "Caută oraș..." : "Fără acces";
    }

    let panouParola = document.getElementById("sectiune-securitate-superadmin");
    if(panouParola) panouParola.style.display = esteAdmin ? "" : "none";
}

/**
 * schimbaPagina(idPagina)
 * Navigheaza catre pagina specificata, actualizeaza meniul si verifica permisiunile.
 * Pentru paginile "esp" si "dispozitive", accesul e restrictionat la admin.
 * @param {string} idPagina - ID-ul paginii: "dashboard", "ventilatie", "lumina", "lacat", "dispozitive", "esp"
 */
function schimbaPagina(idPagina) {
    paginaCurenta = idPagina;
    localStorage.setItem("ultimaPagina", idPagina); 

    let pagini = document.querySelectorAll('.pagina'); pagini.forEach(p => p.classList.remove('activa'));
    document.getElementById('pagina-' + idPagina).classList.add('activa');
    let linkuri = document.querySelectorAll('#sidebar a'); linkuri.forEach(l => l.classList.remove('activ'));
    document.getElementById('link-' + idPagina).classList.add('activ');

    if (idPagina === "dispozitive" || idPagina === "esp") {
        const rol = window.rolUtilizator || localStorage.getItem("rolUtilizator") || "";
        const elContinut = document.getElementById(`continut-${idPagina}`);
        const elEroare   = document.getElementById(`eroare-${idPagina}`);
        if (rol !== "admin") {
            if (elContinut) elContinut.classList.add("fortat-ascuns");
            if (elEroare)   elEroare.style.display = "block";
        } else {
            if (elContinut) elContinut.classList.remove("fortat-ascuns");
            if (elEroare)   elEroare.style.display = "none";
        }
        // Sectiunea de comenzi diagnostic (pagina "Stare Sistem") urmeaza aceeasi regula.
        // Ascunderea din browser e doar estetica: accesul real e verificat pe server in diag.php.
        if (idPagina === "esp") {
            const elComenzi = document.getElementById("continut-comenzi");
            if (elComenzi) elComenzi.classList.toggle("fortat-ascuns", rol !== "admin");
        }
    }
    updatePeriodic();
}

/**
 * toggleMeniu()
 * Deschide sau inchide meniul lateral (sidebar).
 */
function toggleMeniu() { document.getElementById("sidebar").classList.toggle("deschis"); }

// =============================================================================
// 5. FETCH DATE (CITIRE)
// =============================================================================
/**
 * aduceVremea(oras)
 * Interogheaza API-ul Open-Meteo pentru datele meteo ale orasului specificat.
 * Afiseaza temperatura, viteza vantului si conditiile meteo cu iconita corespunzatoare.
 * @param {string} oras - Numele orasului (ex: "Timisoara")
 */
async function aduceVremea(oras) {
    try {
        const raspunsGeo = await fetch(`https://geocoding-api.open-meteo.com/v1/search?name=${oras}&count=1&language=en&format=json`);
        const dateGeo = await raspunsGeo.json();
        if (!dateGeo.results) return;
        let lat = dateGeo.results[0].latitude; let lon = dateGeo.results[0].longitude;
        document.getElementById("vreme-nume-oras").innerText = dateGeo.results[0].name;

        const raspunsVreme = await fetch(`https://api.open-meteo.com/v1/forecast?latitude=${lat}&longitude=${lon}&current_weather=true`);
        const dateVreme = await raspunsVreme.json();

        document.getElementById("vreme-temp").innerText = dateVreme.current_weather.temperature.toFixed(1);
        document.getElementById("vreme-vant").innerText = dateVreme.current_weather.windspeed.toFixed(1);
        
        let cod = dateVreme.current_weather.weathercode;
        let textMeteo = "Necunoscut"; let svgMeteo = "";

        if (cod === 0) { textMeteo = "Senin"; svgMeteo = `<svg class="status-icon" viewBox="0 0 24 24" fill="none" stroke-width="2"><circle cx="12" cy="12" r="5"></circle><line x1="12" y1="1" x2="12" y2="3"></line><line x1="12" y1="21" x2="12" y2="23"></line><line x1="4.22" y1="4.22" x2="5.64" y2="5.64"></line><line x1="18.36" y1="18.36" x2="19.78" y2="19.78"></line><line x1="1" y1="12" x2="3" y2="12"></line><line x1="21" y1="12" x2="23" y2="12"></line><line x1="4.22" y1="19.78" x2="5.64" y2="18.36"></line><line x1="18.36" y1="5.64" x2="19.78" y2="4.22"></line></svg>`; } 
        else if (cod <= 48) { textMeteo = cod < 10 ? "Înnorat" : "Ceață"; svgMeteo = `<svg class="status-icon" viewBox="0 0 24 24" fill="none" stroke-width="2"><path d="M17.5 19H9a7 7 0 1 1 6.71-9h1.79a4.5 4.5 0 1 1 0 9Z"></path></svg>`; } 
        else if (cod <= 67 || (cod >= 80 && cod <= 82)) { textMeteo = "Ploaie"; svgMeteo = `<svg class="status-icon" viewBox="0 0 24 24" fill="none" stroke-width="2"><path d="M20 16.2A4.5 4.5 0 0 0 17.5 8h-1.8A7 7 0 1 0 4 14.9"></path><path d="M16 14v6"></path><path d="M8 14v6"></path><path d="M12 16v6"></path></svg>`; } 
        else if (cod <= 77) { textMeteo = "Ninsoare"; svgMeteo = `<svg class="status-icon" viewBox="0 0 24 24" fill="none" stroke-width="2"><path d="M20 17.58A5 5 0 0 0 18 8h-1.26A8 8 0 1 0 4 16.25"></path><path d="M8 16h.01"></path><path d="M8 20h.01"></path><path d="M12 18h.01"></path><path d="M12 22h.01"></path><path d="M16 16h.01"></path><path d="M16 20h.01"></path></svg>`; } 
        else if (cod >= 95) { textMeteo = "Furtună"; svgMeteo = `<svg class="status-icon" viewBox="0 0 24 24" fill="none" stroke-width="2"><path d="M19 16.9A5 5 0 0 0 18 7h-1.26a8 8 0 1 0-11.62 9"></path><path d="M13 11l-4 6h6l-4 6"></path></svg>`; }
        
        document.getElementById("vreme-status").innerHTML = svgMeteo + "<span style='margin-left: 8px;'>" + textMeteo + "</span>";
    } catch (error) {}
}

/**
 * aduceDateStatus()
 * Citeste starea sistemului de ventilatie de la ESP32 (/status).
 * Actualizeaza modul, temperatura, starea releului si badge-urile din dashboard si pagina ventilatie.
 * In modul DINAMIC afiseaza si numarul de utilizatori conectati.
 */
async function aduceDateStatus() {
    try {
        const r = await fetch(ESP_IP + "/status"); const date = await r.json(); 
        
        if(date.mod) {
            modVentilatieCurent = date.mod;
        } else {
            modVentilatieCurent = "AUTO"; 
        }
        
        let clasaMod = "badge-verde"; 
        if (modVentilatieCurent === "MANUAL ON") clasaMod = "badge-galben";
        else if (modVentilatieCurent === "MANUAL OFF") clasaMod = "badge-rosu";
        else if (modVentilatieCurent === "DINAMIC") clasaMod = "badge-albastru";

        let detaliiDinamic = "";
        if (modVentilatieCurent === "DINAMIC") {
            let textClienti = (date.clienti === 1) ? "1 utilizator" : date.clienti + " utilizatori";
            
            // Calculăm estimativ RPM-ul pe baza procentului (luând 1400 RPM ca turație maximă)
            let estimareRPM = Math.round((date.procent / 100) * 1400);
            let textTuratie = date.procent > 0 ? `${date.procent}% (~${estimareRPM} RPM)` : `0% (Oprit)`;

            detaliiDinamic = `
            <div style="font-size: 14px; margin-top: 12px; margin-bottom: 5px; font-weight: normal; color: #e0e0e0; letter-spacing: 0.5px; line-height: 1.6;">
                <span style="display:block;">👤 <span style="color: #a0a0a0;">Conectați:</span> <b style="color: #0ea5e9;">${textClienti}</b></span>
            </div>`;
        }
        
        let txtMod = `<div style="display: flex; flex-direction: column; align-items: flex-start;"><span class="status-badge ${clasaMod}">${modVentilatieCurent}</span>${detaliiDinamic}</div>`;

        let fanPornit = (date.fan === "true" || date.fan === true);
        let clasaRel = fanPornit ? "badge-verde" : "badge-rosu";
        let txtRel = `<span class="status-badge ${clasaRel}">${fanPornit ? "PORNIT" : "OPRIT"}</span>`;

        document.getElementById("dash-temp").innerText = date.temp.toFixed(1); document.getElementById("vent-temp").innerText = date.temp.toFixed(1);
        document.getElementById("dash-mod").innerHTML = txtMod; 
        document.getElementById("vent-mod").innerHTML = txtMod; 
        document.getElementById("dash-releu").innerHTML = txtRel; 
        document.getElementById("vent-releu").innerHTML = txtRel; 
        document.getElementById("dash-prag").innerText = date.target.toFixed(0); document.getElementById("vent-prag-afisaj").innerText = date.target.toFixed(0);

        const cardVent = document.getElementById("card-status-ventilatie");
        if(cardVent) {
            cardVent.classList.remove("card-active-verde", "card-active-galben", "card-active-rosu");
            if (fanPornit) cardVent.classList.add("card-active-verde");
            else if (modVentilatieCurent.includes("MANUAL")) cardVent.classList.add("card-active-galben");
        }
    } catch (error) {}
}

/**
 * aduceDateLumina()
 * Citeste starea sistemului de iluminat de la ESP32 (/lightStatus).
 * Actualizeaza badge-urile de stare, slider-ul de intensitate si culoarea becului Tapo.
 * Dezactiveaza controalele de culoare/intensitate cand becul e oprit.
 */
async function aduceDateLumina() {
    try {
        const r = await fetch(ESP_IP + "/lightStatus"); const date = await r.json(); 
        let stareGen = ""; let clasaStare = ""; let iconSvg = "";
        const svgAprins = `<svg class="status-icon" viewBox="0 0 24 24" fill="none" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M15 14c.2-1 .7-1.7 1.5-2.5 1-.9 1.5-2.2 1.5-3.5A6 6 0 0 0 6 8c0 1.3.5 2.6 1.5 3.5.8.8 1.3 1.5 1.5 2.5"></path><path d="M9 18h6"></path><path d="M10 22h4"></path><path d="M12 2v1"></path><path d="M12 7v1"></path><path d="M4 10h1"></path><path d="M19 10h1"></path></svg>`;
        const svgStins = `<svg class="status-icon" viewBox="0 0 24 24" fill="none" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M15 14c.2-1 .7-1.7 1.5-2.5 1-.9 1.5-2.2 1.5-3.5A6 6 0 0 0 6 8c0 1.3.5 2.6 1.5 3.5.8.8 1.3 1.5 1.5 2.5"></path><path d="M9 18h6"></path><path d="M10 22h4"></path></svg>`;

        if (date.releuOn === false) { stareGen = "STINS (RELEU OFF)"; clasaStare = "badge-rosu"; iconSvg = svgStins; } 
        else if (date.softOn === false) { stareGen = "STINS (SOFT)"; clasaStare = "badge-galben"; iconSvg = svgStins; } 
        else { stareGen = "APRINS"; clasaStare = "badge-verde"; iconSvg = svgAprins; }

        let htmlLumina = `<span class="status-badge ${clasaStare}">${stareGen}${iconSvg}</span>`;

        if(document.getElementById("dash-lum-stare")) document.getElementById("dash-lum-stare").innerHTML = htmlLumina;
        if(document.getElementById("lum-stare-gen")) document.getElementById("lum-stare-gen").innerHTML = htmlLumina;
        
        let txtReleu = date.releuOn ? `<span class="status-badge badge-verde">TRECE CURENTUL</span>` : `<span class="status-badge badge-rosu">OPRIT</span>`;
        if(document.getElementById("lum-stare-releu")) document.getElementById("lum-stare-releu").innerHTML = txtReleu;
        
        let txtSoft = date.softOn ? `<span class="status-badge badge-verde">PORNIT</span>` : `<span class="status-badge badge-rosu">OPRIT</span>`;
        if(document.getElementById("lum-stare-soft")) document.getElementById("lum-stare-soft").innerHTML = txtSoft;

        const cardLumina = document.getElementById("card-status-lumina");
        if(cardLumina) {
            cardLumina.classList.remove("card-active-verde", "card-active-galben", "card-active-rosu");
            if (date.releuOn === false) cardLumina.classList.add("card-active-rosu");
            else if (date.softOn && date.luminozitate > 0) cardLumina.classList.add("card-active-verde");
            else if (date.softOn === false && date.releuOn === true) cardLumina.classList.add("card-active-galben");
        }

        if(document.getElementById("dash-lum-proc")) document.getElementById("dash-lum-proc").innerText = date.luminozitate;
        if(document.getElementById("dash-lum-culoare-text")) document.getElementById("dash-lum-culoare-text").innerText = date.culoare.toUpperCase();
        if(document.getElementById("dash-lum-culoare-box")) document.getElementById("dash-lum-culoare-box").style.backgroundColor = date.culoare;
        if(document.getElementById("lum-pagina-culoare-text")) {
            document.getElementById("lum-pagina-culoare-text").innerText = date.culoare.toUpperCase();
            document.getElementById("lum-pagina-culoare-box").style.backgroundColor = date.culoare;
        }

        if (document.activeElement !== document.getElementById('input-luminozitate')) {
            let slider = document.getElementById("input-luminozitate");
            if (slider) slider.value = date.luminozitate;
            if(document.getElementById("lum-pagina-intensitate")) document.getElementById("lum-pagina-intensitate").innerText = date.luminozitate;
        }
        if (document.activeElement !== document.getElementById('input-culoare')) {
            let colorInput = document.getElementById("input-culoare");
            if (colorInput) colorInput.value = date.culoare;
        }

        let btnSoft = document.getElementById("btn-soft-lumina");
        let grupCuloare = document.getElementById("grup-culoare-actiune");
        let grupIntensitate = document.getElementById("grup-intensitate-actiune");

        if (date.releuOn === false || date.softOn === false) {
    if(btnSoft) { btnSoft.disabled = false; btnSoft.style.opacity = "1"; btnSoft.style.cursor = "pointer"; }
    if(grupCuloare) { grupCuloare.style.pointerEvents = "none"; grupCuloare.style.opacity = "0.4"; }
    if(grupIntensitate) { grupIntensitate.style.pointerEvents = "none"; grupIntensitate.style.opacity = "0.4"; }
} else {
    if(btnSoft) { btnSoft.disabled = false; btnSoft.style.opacity = "1"; btnSoft.style.cursor = "pointer"; }
    if(grupCuloare) { grupCuloare.style.pointerEvents = "auto"; grupCuloare.style.opacity = "1"; }
    if(grupIntensitate) { grupIntensitate.style.pointerEvents = "auto"; grupIntensitate.style.opacity = "1"; }
}
    } catch (error) {}
}

/**
 * aduceDateLacat()
 * Citeste starea usii de la ESP32 (/doorStatus) si ultimul utilizator din baza de date.
 * Actualizeaza badge-ul de stare si glow-ul cardului.
 */
async function aduceDateLacat() { 
    try { 
        const resDB = await fetch(`${XAMPP_IP}/smarthome/get_settings.php`);
        const setari = await resDB.json();
        let ultimulUser = setari.usa_ultimul_user ? setari.usa_ultimul_user : "Nimeni";
        if(document.getElementById("dash-usa-user")) document.getElementById("dash-usa-user").innerText = ultimulUser;
        if(document.getElementById("lacat-user")) document.getElementById("lacat-user").innerText = ultimulUser;
    } catch(e) {}

    try {
        const r = await fetch(ESP_IP + "/doorStatus"); const d = await r.json(); 
        const svgDeschis = `<svg class="status-icon" viewBox="0 0 24 24" fill="none" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect width="18" height="11" x="3" y="11" rx="2" ry="2"></rect><path d="M7 11V7a5 5 0 0 1 9.9-1"></path></svg>`;
        const svgInchis = `<svg class="status-icon" viewBox="0 0 24 24" fill="none" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect width="18" height="11" x="3" y="11" rx="2" ry="2"></rect><path d="M7 11V7a5 5 0 0 1 10 0v4"></path></svg>`;
        let iconSvg = d.usaDeschisa ? svgDeschis : svgInchis;
        let textStare = d.usaDeschisa ? "DESCHISĂ" : "ÎNCHISĂ"; 
        let clasaStare = d.usaDeschisa ? "badge-verde" : "badge-rosu";
        let htmlUsa = `<span class="status-badge ${clasaStare}">${textStare}${iconSvg}</span>`;

        if(document.getElementById("dash-usa-stare")) document.getElementById("dash-usa-stare").innerHTML = htmlUsa;
        if(document.getElementById("lacat-stare")) document.getElementById("lacat-stare").innerHTML = htmlUsa; 
        const cardUsa = document.getElementById("card-status-usa");
        if(cardUsa) {
            cardUsa.classList.remove("card-active-verde", "card-active-rosu");
            if (d.usaDeschisa) cardUsa.classList.add("card-active-verde");
            else cardUsa.classList.add("card-active-rosu");
        }
    } catch (e) {} 
}

/**
 * aduceDateESP()
 * Citeste informatiile de sistem de la ESP32 (/system).
 * Afiseaza SSID, IP local, uptime si utilizarea memoriei RAM.
 */
async function aduceDateESP() {
    try {
        const r = await fetch(ESP_IP + "/system"); const date = await r.json(); 
        document.getElementById("esp-ssid").innerText = date.ssid; document.getElementById("esp-ip").innerText = date.ip;
        let h = Math.floor(date.uptime / 3600); let m = Math.floor((date.uptime % 3600) / 60); let s = date.uptime % 60;
        document.getElementById("esp-uptime").innerText = `${h}h ${m}m ${s}s`;
        let t = (date.total_heap / 1024).toFixed(0); let f = (date.free_heap / 1024).toFixed(0); let u = t - f; let p = ((u / t) * 100).toFixed(1);
        document.getElementById("esp-ram").innerText = `${u} / ${t} KB (${p}%)`;
    } catch (error) {}
}

/**
 * updatePeriodic()
 * Functia de polling executata la fiecare 2 secunde.
 * Apeleaza doar functiile necesare pentru pagina activa, pentru a minimiza request-urile.
 */
async function updatePeriodic() {
    if (paginaCurenta === "dashboard") { await aduceDateStatus(); await aduceDateLumina(); await aduceDateLacat(); } 
    else if (paginaCurenta === "ventilatie") { await aduceDateStatus(); } 
    else if (paginaCurenta === "lumina") { await aduceDateLumina(); }
    else if (paginaCurenta === "lacat") { await aduceDateLacat(); }
    else if (paginaCurenta === "dispozitive" && window.rolUtilizator === "admin") { 
        if(document.getElementById("vedere-live").style.display !== "none") { await aduceDateClienti(); } 
        else { fetch(ESP_IP + "/clients").catch(e=>{}); } 
    }
    else if (paginaCurenta === "esp" && window.rolUtilizator === "admin") { await aduceDateESP(); }
}

// =============================================================================
// 6. MODULE SUPERADMIN
// =============================================================================
/**
 * aduceDateClienti()
 * Citeste lista de dispozitive conectate din ESP32 si din baza de date.
 * Combina datele pentru a afisa IP, rol, timp de la ultima activitate si status online/offline.
 * Vizibila doar pentru administratori.
 */
async function aduceDateClienti() {
    try {
        const resDB = await fetch(`${XAMPP_IP}/smarthome/gestiune_utilizatori.php?action=get`);
        totiUtilizatoriiDB = await resDB.json();
        let clientiESP = [];
        try { const r = await fetch(ESP_IP + "/clients"); clientiESP = await r.json(); } catch(e) {}

        let tabelLiveHTML = ""; let afisati = 0;
        clientiESP.sort((a, b) => a.secAgo - b.secAgo);
        clientiESP.forEach(client => {
            if(afisati >= 10 || client.ip === "") return;
            let userDinDB = totiUtilizatoriiDB.find(u => u.ip_dispozitiv === client.ip);
            let rol = userDinDB ? userDinDB.rol_acces.toUpperCase() : "VIZITATOR";
            let textTimp = formateazaTimp(client.secAgo);
            let clasaStatus = client.status === "Online" ? "status-online" : "status-offline";
            
            tabelLiveHTML += `<tr>
                <td>${afisati + 1}</td><td>${client.ip}</td><td style="color:#a0a0a0;">${textTimp}</td>
                <td style="color:#0ea5e9; font-weight:bold;">${rol}</td><td class="${clasaStatus}">${client.status}</td>
            </tr>`;
            afisati++;
        });
        if (afisati === 0) tabelLiveHTML = '<tr><td colspan="5" style="text-align:center; color:#a0a0a0;">Niciun dispozitiv conectat.</td></tr>';
        document.getElementById("tabel-date-live").innerHTML = tabelLiveHTML;
    } catch (error) {}
}

/**
 * deschideArhiva()
 * Comuta vizualizarea din modul live in modul arhiva (editare utilizatori).
 * Reseteaza modificarile nesalvate si redeseneaza tabelul.
 */
function deschideArhiva() {
    document.getElementById("vedere-live").style.display = "none";
    document.getElementById("vedere-arhiva").style.display = "block";
    modificariNesalvate = {}; deseneazaTabelArhiva();
}

/**
 * deseneazaTabelArhiva()
 * Genereaza HTML-ul tabelului de gestionare utilizatori.
 * Administratorul curent nu isi poate modifica propriul rol (protectie anti-blocare).
 */
function deseneazaTabelArhiva() {
    let tabelArhivaHTML = "";
    let useriSortati = [...totiUtilizatoriiDB].sort((a, b) => new Date(b.ultima_logare) - new Date(a.ultima_logare));
    useriSortati.forEach(u => {
        let colorMap = { "vizitator": "#ef4444", "proprietar": "#ffb703", "admin": "#22c55e" };
        let selCol = colorMap[u.rol_acces] || "#fff";
        let isCurrentUser = (u.ip_dispozitiv === window.ipUtilizator);
        let disabledRank = isCurrentUser ? "disabled" : "";
        let opacityRank = isCurrentUser ? "opacity: 0.5; cursor: not-allowed;" : "cursor: pointer;";
        let ultimaAparitieFormatata = u.ultima_logare ? formateazaTimp(u.ultima_logare) : "Nedefinit";
        let marcajTu = isCurrentUser ? ' <span style="color:#0ea5e9; font-weight:bold; margin-left: 5px;">(TU)</span>' : '';

        tabelArhivaHTML += `
        <tr>
            <td style="font-family: monospace; font-size:16px;">${u.ip_dispozitiv} 
                <div style="font-size: 11px; color:#888;">Ultima logare: ${ultimaAparitieFormatata}</div>
            </td>
            <td>
                <div style="display: flex; align-items: center;">
                    <input type="text" value="${u.username}" class="input-temp" style="width: 140px; margin: 0; padding: 5px; font-size: 14px; text-align: left;" onchange="inregistreazaModificare('${u.ip_dispozitiv}', 'nume', this.value, this)" placeholder="Nume dispozitiv...">
                    ${marcajTu}
                </div>
            </td>
            <td>
                <select style="padding:5px; background:#121212; color:${selCol}; border:1px solid #555; border-radius:4px; font-weight:bold; ${opacityRank}" onchange="inregistreazaModificare('${u.ip_dispozitiv}', 'rol', this.value, this)" ${disabledRank}>
                    <option value="vizitator" ${u.rol_acces === 'vizitator' ? 'selected' : ''}>VIZITATOR</option>
                    <option value="proprietar" ${u.rol_acces === 'proprietar' ? 'selected' : ''}>PROPRIETAR</option>
                    <option value="admin" ${u.rol_acces === 'admin' ? 'selected' : ''}>ADMIN</option>
                </select>
            </td>
            <td><button class="btn-actiune" style="background-color: #ef4444; border:none; padding: 5px 10px; ${opacityRank}" onclick="stergeUtilizator('${u.ip_dispozitiv}')" ${disabledRank}>🗑️ Șterge</button></td>
        </tr>`;
    });
    document.getElementById("tabel-date-arhiva").innerHTML = tabelArhivaHTML;
}

/**
 * inregistreazaModificare(ip, tip, valoare, elementHTML)
 * Inregistreaza local o modificare de rol sau nume inainte de salvare.
 * Marcheaza vizual elementul modificat (albastru) pana la confirmare.
 * @param {string} ip - IP-ul dispozitivului modificat
 * @param {string} tip - "rol" sau "nume"
 * @param {string} valoare - Noua valoare
 * @param {HTMLElement} elementHTML - Elementul SELECT sau INPUT din tabel
 */
function inregistreazaModificare(ip, tip, valoare, elementHTML) {
    if (!modificariNesalvate[ip]) {
        let userVechi = totiUtilizatoriiDB.find(u => u.ip_dispozitiv === ip);
        modificariNesalvate[ip] = { nume: userVechi.username, rol: userVechi.rol_acces };
    }
    modificariNesalvate[ip][tip] = valoare;
    if(elementHTML.tagName === "SELECT") { elementHTML.style.color = "#0ea5e9"; elementHTML.style.borderColor = "#0ea5e9"; }
    if(elementHTML.tagName === "INPUT") { elementHTML.style.borderColor = "#0ea5e9"; elementHTML.style.color = "#0ea5e9"; }
}

/**
 * salveazaArhiva()
 * Trimite toate modificarile nesalvate la gestiune_utilizatori.php prin POST JSON.
 * Dupa salvare reseteaza starea si reimprospateaza tabelul.
 */
async function salveazaArhiva() {
    if (Object.keys(modificariNesalvate).length === 0) { alert("Nu ai nicio modificare de salvat!"); return; }
    try {
        const raspuns = await fetch(`${XAMPP_IP}/smarthome/gestiune_utilizatori.php?action=update`, {
            method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(modificariNesalvate)
        });
        const date = await raspuns.json();
        if(date.status === "success") { alert("Modificările au fost salvate!"); modificariNesalvate = {}; await aduceDateClienti(); deseneazaTabelArhiva(); }
    } catch (e) { alert("Eroare la salvare!"); }
}

/**
 * inchideArhiva()
 * Revine la vizualizarea live. Daca exista modificari nesalvate, cere confirmare.
 */
function inchideArhiva() {
    if (Object.keys(modificariNesalvate).length > 0) {
        let confirma = confirm("⚠️ ATENȚIE!\n\nAi modificat rank-uri dar nu ai apăsat SALVEAZĂ.\nDacă ieși acum, modificările se vor pierde.\n\nEști sigur că vrei să ieși FĂRĂ să salvezi?");
        if (!confirma) return; 
    }
    document.getElementById("vedere-arhiva").style.display = "none";
    document.getElementById("vedere-live").style.display = "block";
    aduceDateClienti(); 
}

/**
 * stergeUtilizator(ip)
 * Sterge un dispozitiv din baza de date dupa confirmare.
 * Administratorul curent nu se poate sterge pe sine (protectie anti-blocare).
 * @param {string} ip - IP-ul dispozitivului de sters
 */
async function stergeUtilizator(ip) {
    if(ip === window.ipUtilizator) { alert("⛔ Acțiune blocată: Trebuie să existe cel puțin un Admin în sistem!"); return; }
    let confirma = confirm(`❗ Ești absolut sigur că vrei să ștergi IP-ul ${ip} din sistem?`);
    if (confirma) {
        try {
            await fetch(`${XAMPP_IP}/smarthome/gestiune_utilizatori.php?action=delete&ip=${ip}`);
            alert("Dispozitiv șters!"); await aduceDateClienti(); deseneazaTabelArhiva();
        } catch (e) { alert("Eroare la ștergere!"); }
    }
}

/**
 * schimbaCredentiale()
 * Actualizeaza user-ul si parola globale prin update_settings.php.
 * Parola este hash-uita automat pe server. Afecteaza toti utilizatorii sistemului.
 */
async function schimbaCredentiale() {
    let userNou = document.getElementById("input-new-user").value; let passNou = document.getElementById("input-new-pass").value;
    if (userNou.trim() === "" || passNou.trim() === "") { alert("⛔ Te rog să completezi ambele câmpuri!"); return; }
    let confirma = confirm(`❗ ATENȚIE!\nVrei să schimbi datele de logare pentru toată lumea?\n\nNou user: ${userNou}\nNouă parolă: ${passNou}`);
    if (confirma) {
        try {
            await salveazaInDB('global_user', userNou); await salveazaInDB('global_pass', passNou);
            alert("✅ Credențialele au fost schimbate!");
            document.getElementById("input-new-user").value = ""; document.getElementById("input-new-pass").value = "";
        } catch(e) { alert("Eroare la salvarea credențialelor!"); }
    }
}

/**
 * deschideJurnalVentilatie()
 * Incarca si afiseaza ultimele 10 actiuni din jurnalul de ventilatie.
 * Marcheaza actiunile efectuate de utilizatorul curent cu eticheta "(Tu)".
 */
async function deschideJurnalVentilatie() {
    document.getElementById("tabel-jurnal-vent").style.display = "block";
    document.getElementById("grup-jurnal-vent").style.display = "none";
    try {
        const r = await fetch(`${XAMPP_IP}/smarthome/jurnal_ventilatie.php?action=get`); const date = await r.json(); let html = "";
        if(date.length === 0) { html = '<tr><td colspan="5" style="text-align:center;">Nicio acțiune înregistrată încă.</td></tr>'; } 
        else {
            date.forEach(rand => {
                let marcajTu = (rand.ip_utilizator === window.ipUtilizator) ? ' <span style="color:#0ea5e9; font-weight:bold;">(Tu)</span>' : '';
                let culoareModBaza = rand.mod_vent.includes('MANUAL') ? '#ffb703' : '#0ea5e9';
                let textModDisplay = rand.mod_vent.replace('(PORNIT)', '<span style="color:#22c55e;">(PORNIT)</span>').replace('(OPRIT)', '<span style="color:#ef4444;">(OPRIT)</span>');
                html += `<tr><td style="color:#a0a0a0; font-size: 14px;">${rand.data_ora}</td><td style="font-family: monospace;">${rand.ip_utilizator}${marcajTu}</td><td style="color:#ffb703;">${rand.temperatura} °C</td><td style="font-weight:bold; color:${culoareModBaza}">${textModDisplay}</td><td style="color:#22c55e;">${rand.prag} °C</td></tr>`;
            });
        }
        document.getElementById("continut-tabel-jurnal").innerHTML = html;
    } catch(e) { document.getElementById("continut-tabel-jurnal").innerHTML = '<tr><td colspan="5" style="text-align:center; color:#ef4444;">Eroare la încărcarea jurnalului!</td></tr>'; }
}

/**
 * inchideJurnalVentilatie()
 * Ascunde tabelul de jurnal si reafiseaza butonul de deschidere.
 */
function inchideJurnalVentilatie() {
    document.getElementById("tabel-jurnal-vent").style.display = "none";
    document.getElementById("grup-jurnal-vent").style.display = "block";
}

// =============================================================================
// COMENZI DIAGNOSTIC (pagina "Stare Sistem") - ecran ILI9341 (SPI) + senzor DHT11
// Browserul NU vorbeste direct cu ESP32: trimite comanda la diag.php (POST), care verifica
// pe server sesiunea si rolul admin, valideaza comanda si abia apoi o transmite la ESP32.
// =============================================================================
const TEXT_HELP = [
    "Comenzi disponibile (10 + reset):",
    "ECRAN ILI9341 (SPI) - scrie codul hex: 29h, 0x29 sau 29",
    "  04h  citește ID-ul afișajului (așteptat 00 93 41)",
    "  0Ah  citește modul de alimentare (power mode)",
    "  0Ch  citește formatul pixelilor (așteptat 0x05 sau 0x55 = 16 biți/pixel)",
    "  28h  Display OFF (oprește afișajul)",
    "  29h  Display ON (pornește afișajul)",
    "SENZOR DHT11 (un singur fir, nu I2C):",
    "  dht.temp  citește temperatura",
    "  dht.hum   citește umiditatea",
    "  dht.all   temperatură + umiditate + index de căldură",
    "  dht.raw   cadrul brut de 40 de biți + checksum",
    "  dht.test  autotest: 3 citiri consecutive (durează ~3 s)",
    "ALTELE:",
    "  reset  readuce ecranul și senzorul la starea inițială (la fel ca butonul Reset)",
    "  help   afișează acest mesaj"
];

let comandaInCurs = false;
const istoricTastat = [];
let indexIstoric = -1;

/** Adauga o linie in consola. tip: "cmd" | "titlu" | "info" | "ok" | "eroare" | "avert" */
function scrieInConsola(text, tip) {
    const c = document.getElementById("consola-diag");
    if (!c) return;
    const linie = document.createElement("div");
    linie.className = "linie-consola linie-" + (tip || "info");
    linie.textContent = text;   // textContent => nu se interpreteaza HTML (fara injectare)
    c.appendChild(linie);
    c.scrollTop = c.scrollHeight;
}

function curataConsola() {
    const c = document.getElementById("consola-diag");
    if (c) c.innerHTML = "";
}

function blocheazaButoaneComenzi(blocat) {
    comandaInCurs = blocat;
    ["btn-trimite-cmd", "btn-reset-cmd"].forEach(id => {
        const b = document.getElementById(id);
        if (b) b.disabled = blocat;
    });
    const inp = document.getElementById("input-comanda");
    if (inp) inp.disabled = blocat;
}

/**
 * trimiteComanda(textForțat)
 * Citeste comanda din caseta (sau o primeste ca parametru), o trimite la diag.php si
 * afiseaza raspunsul in consola in formatul:
 *   Comanda 29h – Display ON (pornește afișajul)
 *   ...linii de detaliu...
 *   Status execuție: OK / EROARE – motiv / NEVERIFICAT – motiv
 */
async function trimiteComanda(textFortat) {
    if (comandaInCurs) return;
    const inp = document.getElementById("input-comanda");
    const text = (typeof textFortat === "string" ? textFortat : (inp ? inp.value : "")).trim();
    if (text === "") return;

    if (typeof textFortat !== "string" && inp) {
        istoricTastat.push(text);
        indexIstoric = istoricTastat.length;
        inp.value = "";
    }
    scrieInConsola("> " + text, "cmd");

    // "help" se rezolva local, fara sa deranjeze ESP32
    if (text.toLowerCase() === "help") {
        TEXT_HELP.forEach(l => scrieInConsola(l, "info"));
        return;
    }

    blocheazaButoaneComenzi(true);
    scrieInConsola("Se execută...", "info");
    const consola = document.getElementById("consola-diag");
    const linieAsteptare = consola.lastChild;
    try {
        const raspuns = await fetch(`${XAMPP_IP}/smarthome/diag.php`, {
            method: "POST",
            headers: { "Content-Type": "application/x-www-form-urlencoded" },
            body: "c=" + encodeURIComponent(text),
            credentials: "same-origin"
        });
        let d = null, textBrut = "";
        try { textBrut = await raspuns.text(); d = JSON.parse(textBrut); } catch (e) { d = null; }
        if (linieAsteptare && linieAsteptare.parentNode) linieAsteptare.remove();

        if (raspuns.status === 403) {
            scrieInConsola("Acces interzis: sesiune expirată sau drepturi insuficiente.", "eroare");
            scrieInConsola("Deconectează-te și autentifică-te din nou ca administrator.", "avert");
        } else if (!d || !d.status) {
            scrieInConsola("Răspuns invalid de la server (HTTP " + raspuns.status + ").", "eroare");
            scrieInConsola("Text primit: " + (textBrut.trim().substring(0, 300) || "(gol)"), "avert");
        } else {
            afiseazaRaspunsComanda(d, text);
            if (d.logat) {
                const tabel = document.getElementById("tabel-jurnal-cmd");
                if (tabel && tabel.style.display === "block") deschideJurnalComenzi();
            }
        }
    } catch (e) {
        if (linieAsteptare && linieAsteptare.parentNode) linieAsteptare.remove();
        scrieInConsola("Nu pot contacta serverul XAMPP. Verifică conexiunea.", "eroare");
    }
    blocheazaButoaneComenzi(false);
    if (inp) inp.focus();
}

/** Formateaza in consola raspunsul JSON primit de la diag.php. */
function afiseazaRaspunsComanda(d, textTastat) {
    const nume = d.comanda || textTastat;
    if (d.descriere) scrieInConsola("Comanda " + nume + " – " + d.descriere, "titlu");
    else             scrieInConsola("Comanda " + nume, "titlu");
    (d.linii || []).forEach(l => scrieInConsola("  " + l, "info"));
    const motiv = d.motiv ? " – " + d.motiv : "";
    if (d.status === "OK") {
        scrieInConsola("Status execuție: OK" + (d.ms ? "  (" + d.ms + " ms)" : ""), "ok");
    } else if (d.status === "NEVERIFICAT") {
        scrieInConsola("Status execuție: NEVERIFICAT" + motiv, "avert");
    } else {
        scrieInConsola("Status execuție: EROARE" + motiv, "eroare");
    }
}

/** Butonul "Reset la valori implicite" = comanda "reset" (cu confirmare). */
function resetareDiagnostic() {
    if (comandaInCurs) return;
    if (!confirm("Resetezi ecranul și senzorul DHT11 la starea inițială?")) return;
    trimiteComanda("reset");
}

// Sageti sus/jos in caseta: reapeleaza comenzile tastate anterior (ca intr-un terminal)
document.addEventListener("keydown", function (ev) {
    const inp = document.getElementById("input-comanda");
    if (!inp || document.activeElement !== inp || istoricTastat.length === 0) return;
    if (ev.key === "ArrowUp") {
        ev.preventDefault();
        indexIstoric = Math.max(0, indexIstoric - 1);
        inp.value = istoricTastat[indexIstoric];
    } else if (ev.key === "ArrowDown") {
        ev.preventDefault();
        indexIstoric = Math.min(istoricTastat.length, indexIstoric + 1);
        inp.value = indexIstoric === istoricTastat.length ? "" : istoricTastat[indexIstoric];
    }
});

/** Escapare HTML pentru valorile afisate in tabel (rezultatele vin din server). */
function escHtml(t) {
    return String(t == null ? "" : t).replace(/[&<>"']/g, ch =>
        ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[ch]));
}

/**
 * deschideJurnalComenzi()
 * Incarca si afiseaza ultimele 10 comenzi din tabelul log_comenzi (prin jurnal_comenzi.php).
 * Acelasi principiu ca la jurnalul de ventilatie; comenzile utilizatorului curent au "(Tu)".
 */
async function deschideJurnalComenzi() {
    document.getElementById("tabel-jurnal-cmd").style.display = "block";
    document.getElementById("grup-jurnal-cmd").style.display = "none";
    const corp = document.getElementById("continut-tabel-cmd");
    try {
        const r = await fetch(`${XAMPP_IP}/smarthome/jurnal_comenzi.php`, { credentials: "same-origin" });
        const date = await r.json();
        if (!Array.isArray(date)) {
            corp.innerHTML = `<tr><td colspan="6" style="text-align:center; color:#ef4444;">${escHtml(date.mesaj || "Eroare la încărcarea istoricului!")}</td></tr>`;
            return;
        }
        let html = "";
        if (date.length === 0) {
            html = '<tr><td colspan="6" style="text-align:center;">Nicio comandă înregistrată încă.</td></tr>';
        } else {
            date.forEach(rand => {
                const marcajTu = (rand.ip_utilizator === window.ipUtilizator) ? ' <span style="color:#0ea5e9; font-weight:bold;">(Tu)</span>' : '';
                const culoare = rand.status === "OK" ? "#22c55e" : (rand.status === "NEVERIFICAT" ? "#ffb703" : "#ef4444");
                html += `<tr><td style="color:#a0a0a0; font-size: 14px;">${escHtml(rand.data_ora)}</td>` +
                        `<td style="font-family: monospace;">${escHtml(rand.ip_utilizator)}${marcajTu}</td>` +
                        `<td>${escHtml(rand.rol)}</td>` +
                        `<td style="font-family: monospace; font-weight:bold; color:#0ea5e9;">${escHtml(rand.comanda)}</td>` +
                        `<td style="font-size: 13px;">${escHtml(rand.rezultat)}</td>` +
                        `<td style="font-weight:bold; color:${culoare};">${escHtml(rand.status)}</td></tr>`;
            });
        }
        corp.innerHTML = html;
    } catch (e) {
        corp.innerHTML = '<tr><td colspan="6" style="text-align:center; color:#ef4444;">Eroare la încărcarea istoricului!</td></tr>';
    }
}

/** Ascunde tabelul de istoric comenzi si reafiseaza butonul de deschidere. */
function inchideJurnalComenzi() {
    document.getElementById("tabel-jurnal-cmd").style.display = "none";
    document.getElementById("grup-jurnal-cmd").style.display = "block";
}

/**
 * inregistreazaInJurnal(temp, mod, prag)
 * Salveaza o intrare in jurnalul de ventilatie prin jurnal_ventilatie.php.
 * Apelata automat la fiecare schimbare de mod sau prag.
 * @param {string} temp - Temperatura curenta
 * @param {string} mod  - Modul de ventilatie (ex: "AUTO (PORNIT)")
 * @param {string} prag - Pragul de temperatura setat
 */
async function inregistreazaInJurnal(temp, mod, prag) {
    let ip = window.ipUtilizator || "IP_Necunoscut";
    try { await fetch(`${XAMPP_IP}/smarthome/jurnal_ventilatie.php?action=add&ip=${ip}&temp=${temp}&mod=${mod}&prag=${prag}`); } catch(e) {}
}

// =============================================================================
// 6b. GESTIONARE SESIUNE - bazata pe DB (activ=1/0 in tabela utilizatori)
// =============================================================================
// Principiu:
//   Login  => login.php seteaza activ=1 automat
//   Logout => apelam sesiune_activa.php?action=logout&token=X => activ=0
//   ESP    => citeste sesiune_activa.php?action=count => nr useri activi din DB
//
// Nu mai depindem de ESP RAM, heartbeat, sau request-uri catre placa.

/**
 * setezaSesiuneActiva(activ)
 * Actualizeaza coloana `activ` din baza de date pentru utilizatorul curent.
 * activ=true  => login  (utilizatorul e prezent, conteaza pentru modul DINAMIC)
 * activ=false => logout (utilizatorul a plecat, ventilatorul scade turatie)
 * Foloseste sendBeacon la deconectare pentru a garanta trimiterea chiar la inchiderea tab-ului.
 * @param {boolean} activ - true la login, false la logout
 */
function setezaSesiuneActiva(activ) {
    // activ=true  => login  (deja facut de login.php, dar il apelam si din JS pentru siguranta)
    // activ=false => logout => seteaza activ=0 in DB
    const token = getDeviceToken();
    if (!token) return;
    const action = activ ? 'login' : 'logout';
    const url = `${XAMPP_IP}/smarthome/sesiune_activa.php?action=${action}&token=${encodeURIComponent(token)}`;
    // Folosim sendBeacon la logout ca sa fie garantat chiar si la inchidere tab
    if (!activ && navigator.sendBeacon) {
        navigator.sendBeacon(url);
    } else {
        fetch(url).catch(() => {});
    }
}

// Stub-uri goale pentru compatibilitate cu apelurile existente din cod
function trimiteHeartbeat() {}
function trimiteClientPleaca() { setezaSesiuneActiva(false); }
function inregistreazaLogare() { setezaSesiuneActiva(true); }
function pornesteListeneriSesiune() {}
function handleVisibilityChange() {}
function handleBeforeUnload() {}

// =============================================================================
// 7. ACȚIUNI ȘI SALVARE (SCRIERE CĂTRE ESP / XAMPP)
// =============================================================================
/**
 * salveazaInDB(nume, valoare)
 * Salveaza o setare in baza de date prin update_settings.php.
 * Folosita pentru toate setarile: ventilatie, lumina, usa, oras vreme, credentiale.
 * @param {string} nume   - Cheia setarii (ex: "vent_mod", "lumina_intensitate")
 * @param {string} valoare - Valoarea de salvat
 */
async function salveazaInDB(nume, valoare) {
    try {
        let valoareSigura = encodeURIComponent(valoare); 
        let urlTarget = `${XAMPP_IP}/smarthome/update_settings.php?nume=${nume}&valoare=${valoareSigura}`;
        let raspuns = await fetch(urlTarget, { cache: "no-store" });
        let dateServer = await raspuns.json(); 
        if (dateServer.status === "success") { console.log(`%c✅ [DB] Salvare cu succes: ${nume} = ${valoare}`, "color: #22c55e; font-weight: bold;"); }
    } catch (e) { console.error("❌ Eroare JS la conexiunea cu Baza de Date:", e); }
}

/**
 * schimbaOrasVreme()
 * Citeste orasul din input, actualizeaza widgetul meteo si salveaza in baza de date.
 */
function schimbaOrasVreme() { 
    let input = document.getElementById("input-oras-vreme"); let oras = input.value; 
    if (oras.trim() !== "") { aduceVremea(oras); salveazaInDB('oras_vreme', oras); input.value = ""; input.blur(); }
}

/**
 * actioneazaLacat()
 * Trimite comanda de deschidere/inchidere usii la ESP32 (/toggleDoor).
 * Dupa 1.5 secunde citeste noua stare si o salveaza in baza de date impreuna cu utilizatorul care a actionat.
 */
async function actioneazaLacat() { 
    try {
        await fetch(ESP_IP + "/toggleDoor"); 
        setTimeout(async () => {
            const r = await fetch(ESP_IP + "/doorStatus"); const d = await r.json();
            salveazaInDB('usa_status', d.usaDeschisa ? "OPEN" : "CLOSED"); 
            let ipUser = window.ipUtilizator || "IP_Necunoscut"; let rolUser = window.rolUtilizator ? window.rolUtilizator.toUpperCase() : "VIZITATOR";
            salveazaInDB('usa_ultimul_user', `${ipUser} - ${rolUser}`); aduceDateLacat();
        }, 1500); 
    } catch(e) {}
}

/**
 * seteazaTemperatura()
 * Trimite pragul de temperatura la ESP32 (/setTarget) si il salveaza in baza de date.
 * Dupa confirmare inregistreaza actiunea in jurnalul de ventilatie.
 */
async function seteazaTemperatura() { 
    let val = document.getElementById("input-prag").value; 
    try { 
        await fetch(ESP_IP + "/setTarget?val=" + val); salveazaInDB('vent_prag', val); 
        setTimeout(async () => {
            const r = await fetch(ESP_IP + "/status"); const d = await r.json();
            let stareReleu = d.fan ? ' (PORNIT)' : ' (OPRIT)';
            inregistreazaInJurnal(d.temp.toFixed(1), (modVentilatieCurent) + stareReleu, d.target.toFixed(0));
        }, 1500);
        aduceDateStatus(); 
    } catch(e) {}
}

// MAGIA ESTE AICI: Trimitem comanda DIRECT catre ESP, in loc de functia /toggle veche
/**
 * schimbaMod()
 * Cicleaza modul de ventilatie: AUTO -> MANUAL ON -> MANUAL OFF -> DINAMIC -> AUTO.
 * Trimite noul mod la ESP32 si il salveaza in baza de date.
 * Inregistreaza schimbarea in jurnalul de ventilatie.
 */
async function schimbaMod() { 
    try {
        // Punem clar cele 4 moduri
        const moduri = ["AUTO", "MANUAL ON", "MANUAL OFF", "DINAMIC"];
        let index = moduri.indexOf(modVentilatieCurent);
        if(index === -1) index = 0; // Dacă în DB era ceva vechi, o ia de la capăt
        index = (index + 1) % moduri.length; 
        let noulMod = moduri[index];
        
        modVentilatieCurent = noulMod; // Actualizăm rapid pe site

        // 1. Trimitem comanda către ESP32 (encodeURIComponent rezolvă spațiul din "MANUAL ON")
        await fetch(ESP_IP + "/setVentMode?mod=" + encodeURIComponent(noulMod)); 

        // 2. Salvăm în baza de date
        salveazaInDB('vent_mod', noulMod); 
        
        // 3. Așteptăm puțin să proceseze și aducem starea
        setTimeout(async () => { 
            aduceDateStatus(); 
            const r = await fetch(ESP_IP + "/status"); const d = await r.json(); 
            let fanPornit = (d.fan === "true" || d.fan === true);
            let stareReleu = fanPornit ? ' (PORNIT)' : ' (OPRIT)';
            inregistreazaInJurnal(d.temp.toFixed(1), noulMod + stareReleu, d.target.toFixed(0));
        }, 1000);
    } catch(e) {}
}

/**
 * toggleReleuLumina()
 * Comuta releul fizic al luminii (taie/da curentul la priza becului).
 * Salveaza noua stare in baza de date dupa confirmare de la ESP32.
 */
async function toggleReleuLumina() { 
    try {
        await fetch(ESP_IP + "/toggleLightRelay"); 
        setTimeout(async () => { const r = await fetch(ESP_IP + "/lightStatus"); const d = await r.json(); salveazaInDB('lumina_releu', d.releuOn ? 'ON' : 'OFF'); }, 1500); aduceDateLumina(); 
    } catch(e) {}
}

/**
 * toggleSoftLumina()
 * Porneste sau opreste becul Tapo prin comanda soft (fara a taia curentul fizic).
 * Salveaza noua stare in baza de date dupa confirmare de la ESP32.
 */
async function toggleSoftLumina() { 
    try {
        await fetch(ESP_IP + "/toggleLightSoft"); 
        setTimeout(async () => { const r = await fetch(ESP_IP + "/lightStatus"); const d = await r.json(); salveazaInDB('lumina_bec', d.softOn ? 'ON' : 'OFF'); }, 1500); aduceDateLumina(); 
    } catch(e) {}
}

/**
 * setareParametriLumina()
 * Trimite intensitatea si culoarea becului Tapo la ESP32 (/setLightParams).
 * Salveaza ambii parametri in baza de date.
 */
async function setareParametriLumina() { 
    let lum = document.getElementById("input-luminozitate").value; let cul = document.getElementById("input-culoare").value;
    try { await fetch(ESP_IP + "/setLightParams?lum=" + lum + "&cul=" + encodeURIComponent(cul)); salveazaInDB('lumina_intensitate', lum); salveazaInDB('lumina_culoare', cul); aduceDateLumina(); } catch(e) {}
}

// =============================================================================
// 8. INITIALIZARE LA INCARCARE
// =============================================================================
window.onload = function() {
    let inputPass = document.getElementById("input-pass");
    if(inputPass) { inputPass.addEventListener("keypress", function(e) { if (e.key === "Enter") { e.preventDefault(); verificaLogin(); } }); }
    let rolSalvat = localStorage.getItem("rolUtilizator"); let ipSalvat = localStorage.getItem("ipUtilizator");
    if (rolSalvat && ipSalvat) {
        window.rolUtilizator = rolSalvat; window.ipUtilizator = ipSalvat; window.numeUtilizator = localStorage.getItem("numeUtilizator") || "Vizitator";
        document.getElementById("ecran-login").style.display = "none"; document.getElementById("ecran-aplicatie").style.display = "block";
        let titlu = document.querySelector(".titlu-pagina"); if(titlu) titlu.innerText = `Smart Home - ${rolSalvat.toUpperCase()}`;
        aplicaPermisiuni();
        
        fetch(`${XAMPP_IP}/smarthome/get_settings.php`).then(r => r.json()).then(setariDB => {
            if (setariDB.oras_vreme) aduceVremea(setariDB.oras_vreme); else aduceVremea("Timișoara");
        }).catch(() => aduceVremea("Timișoara"));
        
        let paginaSalvata = localStorage.getItem("ultimaPagina") || "dashboard"; schimbaPagina(paginaSalvata);
        updatePeriodic(); loopDate = setInterval(updatePeriodic, 2000);
        setezaSesiuneActiva(true); // re-confirma activ=1 la revenire pe pagina
        pornesteListeneriSesiune(); // sesiune robusta (throttle fix)
    }
};