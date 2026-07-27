# Scherm koppelen (pairen)

> **Tijdsindicatie:** Het hele proces duurt ~2 minuten per scherm.
> Het scherm is maar **30 seconden** in pair mode — lees dit eerst helemaal door voor je begint.

---

## Wat er onder de motorkap gebeurt

De ESP32 doet alsof hij een Somfy/Velux afstandsbediening is.  
Je maakt eerst een **virtueel remote-adres** aan (een uniek 3-byte ID).  
Daarna vertel je het scherm: *"Accepteer dit adres als controller"* — dat is het Add-frame.  
Vanaf dat moment reageert het scherm op open/stop/close commando's van de ESP32.

```
[Scherm]  ←── 868 MHz Add-frame ───  [ESP32 gateway]
  ↓ accepteert
[Scherm]  ←── open/close/stop ──────  [ESP32 gateway]
          ──── positie update ──────→  [Home Assistant]
```

---

## Voorbereiding — éénmalig

Deze stap doe je één keer voor alle schermen.

### 1. Fysieke afstandsbediening bij de hand

Je hebt de **originele Somfy of Velux remote** van het scherm nodig.  
Zonder die remote kun je het scherm niet in pair mode zetten.

### 2. Controleer of de gateway werkt

In Home Assistant → **Instellingen → Apparaten & diensten → ESPHome → iohomecontrol**

Kijk of de entiteit **Gateway status** de tekst `Ready` toont.  
Zie je `Radio init failed` → stop hier, zie [Troubleshooting](Troubleshooting).

---

## Stap-voor-stap: één scherm koppelen

### ① Naam invoeren voor het scherm

**Waar:** HA → iohomecontrol apparaat → entiteit **"Scherm naam"** (tekstveld)

Vul de naam in die je het scherm wilt geven, bijvoorbeeld:
```
Slaapkamer links
```

Druk op Enter of klik het vinkje.  
De naam wordt opgeslagen in de gateway — dit is de naam waarmee de Cover entiteit straks verschijnt in HA.

> ⚠️ **Nog niet op knoppen drukken.** Naam eerst, daarna pas de andere stappen.

---

### ② Virtueel remote-adres aanmaken

**Waar:** HA → iohomecontrol → knop **"Maak nieuw virtueel remote"**

Druk één keer op deze knop.  
De gateway genereert een uniek 3-byte adres en slaat het op in `/1W.json` op de ESP32.  
Je hoeft het adres zelf niet te weten.

> ℹ️ Elk scherm krijgt zijn eigen adres. Druk deze knop opnieuw voor elk nieuw scherm.

---

### ③ Scherm in pair mode zetten  ⏱ doe dit vlak voor stap ④

**Wat je nodig hebt:** de originele afstandsbediening van het scherm.

1. Houd de afstandsbediening dicht bij het scherm (<1 meter)
2. Druk het **kleine knopje achterop** de afstandsbediening  
   (soms onder een klepje, soms met een pen te bedienen)
3. Het scherm beweegt kort **omhoog en omlaag** — dit bevestigt pair mode
4. Je hebt nu **~30 seconden** voor stap ④, ⑤ en ⑥

> ⚠️ **Timing is kritiek.** Doe stap ④–⑥ binnen 30 seconden.  
> Reageert het scherm niet? Wacht 10 seconden en probeer opnieuw.

---

### ④ Adres detecteren via Scan

**Waar:** HA → iohomecontrol → knop **"Scan — luister naar schermen"**

Druk op deze knop. De gateway gaat in luistermodus.  
Het scherm zendt zijn adres uit terwijl het in pair mode staat.  

Na een paar seconden verschijnt een hexadecimaal adres (6 tekens) in:
- **"Laatste gehoord adres"** sensor
- **"Scherm in pair mode"** sensor

Bijvoorbeeld: `A1B2C3`

> ⚠️ Zie je geen adres? Controleer of het scherm nog in pair mode staat (stap ③ opnieuw).  
> Meerdere schermen in pair mode tegelijk → alleen het sterkste signaal wordt getoond.

---

### ⑤ Adres als doel instellen

**Optie A — automatisch (aanbevolen):**  
HA → iohomecontrol → knop **"Gebruik laatste adres als doel"**  
→ Het adres uit stap ④ wordt automatisch in **"Doel scherm ID"** gezet.

**Optie B — handmatig:**  
HA → iohomecontrol → tekstveld **"Doel scherm ID"**  
→ Typ het 6-tekens adres in dat je in stap ④ zag.

> ⚠️ Zorg dat het juiste adres staat ingevuld vóór je naar stap ⑥ gaat.

---

### ⑥ Koppelen

**Waar:** HA → iohomecontrol → knop **"Koppel scherm"**

Druk op deze knop. De ESP32 stuurt een **1W Add-frame** over 868 MHz naar het scherm.  
Het scherm accepteert de gateway als controller en **beweegt kort op en neer** als bevestiging.

> ℹ️ Geen bevestiging (scherm beweegt niet)?  
> → Pair mode verlopen. Start opnieuw bij stap ③.

---

### ⑦ Cover entiteit verschijnt in HA

Na een geslaagde koppeling:
1. De **Gateway status** sensor toont `Paired: A1B2C3`
2. Een nieuwe Cover entiteit `iohc_A1B2C3` verschijnt in HA
3. Test direct: druk op omhoog/omlaag in de cover tile

> ℹ️ Verschijnt de entiteit niet meteen? Stuur één commando (open of close) — dat triggert de registratie.  
> Nog steeds niet? Herstart HA (niet de ESP32).

---

## Samenvatting — spiekbriefje

```
① Scherm naam invoeren
② Nieuw virtueel remote aanmaken
③ Knopje achterop originele remote → scherm beweegt op/neer
④ Scan knop → wacht op adres in "Laatste gehoord adres"
⑤ "Gebruik laatste adres als doel" knop
⑥ "Koppel scherm" knop → scherm beweegt op/neer = succes
⑦ Cover entiteit verschijnt in HA
```

> ⏱ Stap ③ t/m ⑥ moet binnen **30 seconden** klaar zijn.

---

## Tweede (en volgende) schermen koppelen

Voor elk nieuw scherm:
1. Herhaal stap ① t/m ⑦ met een nieuwe naam
2. Stap ② aanmaken van een remote is voor **elk scherm apart** nodig
3. Elk scherm krijgt zijn eigen `iohc_<adres>` entiteit in HA

---

## Scherm ontkoppelen

1. Vul het scherm-adres in bij **"Doel scherm ID"** (bijv. `A1B2C3`)
2. Zet het scherm in pair mode (stap ③)
3. Druk op **"Ontkoppel scherm"**
4. Het scherm beweegt ter bevestiging

> ℹ️ De Cover entiteit in HA verdwijnt na een herstart van HA.

---

## Koppelen via HA automatisering

Je kunt het koppelproces aansturen vanuit een script of automatisering:

```yaml
service: esphome.iohomecontrol_pair_screen
data:
  screen_name: "Slaapkamer links"
  screen_id: "A1B2C3"   # het adres uit stap ④
```

---

## Referentie: serieel ↔ ESPHome vertaaltabel

| Oud serieel commando | ESPHome actie in HA | Omschrijving |
|---|---|---|
| `new1W Slaapkamer links` | Naam invoeren + **Nieuw virtueel remote** | Remote-adres aanmaken |
| `scan` | **Scan** knop | Luistermodus starten |
| `lastAddr` | **Laatste gehoord adres** sensor | Huidig gehoord adres |
| `add Slaapkamer links` | **Koppel scherm** knop | Add-frame sturen |
| `remove Slaapkamer links` | **Ontkoppel scherm** knop | Remove-frame sturen |
| `open Slaapkamer links` | Cover → omhoog | Scherm omhoog |
| `close Slaapkamer links` | Cover → omlaag | Scherm omlaag |
| `stop Slaapkamer links` | Cover → stop | Scherm stoppen |
| `position Slaapkamer links 50` | Cover slider op 50% | Positie instellen |
| `list1W` | Alle `iohc_*` entiteiten in HA | Overzicht gekoppelde schermen |
