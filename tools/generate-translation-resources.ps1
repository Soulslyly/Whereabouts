[CmdletBinding()]
param([string]$ProjectRoot)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }
$catalogPath = Join-Path $ProjectRoot 'src/UI/TranslationCatalog.inc'
$entries = @()
foreach ($line in Get-Content -LiteralPath $catalogPath -Encoding utf8) {
    if ($line -notmatch '^WA_TEXT\(([^,]+),\s*"([^"]*)"\)$') { throw "Invalid catalog row: $line" }
    $entries += [pscustomobject]@{ Key = '$Whereabouts_' + $Matches[1]; English = $Matches[2] }
}

$common = @{
    FRENCH = @('Rechercher','PNJ suivis','Favoris','Récents','Paramètres','Rechercher et cibler','Nom ou ID du PNJ','Clavier virtuel','Utiliser la cible de la console','Utiliser la cible du réticule','Effacer la sélection','Vouliez-vous dire :','Filtres et tri','Filtres de texte','Le plugin contient','Le lieu contient','État du PNJ','Tous','Oui','Non','Vivant','Mort','Activé','Désactivé','Compagnon','Compagnon potentiel','Chargé','Non chargé','Listes et contexte','Favoris uniquement','Suivis uniquement','Même lieu','Inclure les PNJ génériques','Ordre','Trier par','Croissant','Effacer les filtres','Nom','Plugin','Niveau','Lieu','Distance','PNJ sélectionné','Aucun PNJ sélectionné.','Commandes','Copier ID','FormID','EditorID','Stable','Détails du PNJ','Identité','État','Sélection de cible','Sélection auto de la cible console','Sélection auto de la cible du réticule','Affichage et recherche','Disposition du clavier virtuel','Alphabétique','Unité de distance','Mètres','Pieds','Unités du jeu','Détail des résultats','Détaillé','Compact','Suivi','Notifier si un PNJ suivi meurt','Maintenance','Actualiser index de recherche','Reconstruire les données de recherche','Préparer la désinstallation','Aléatoire','Direction','Décroissant','Non utilisé','L''ordre aléatoire n''utilise pas de direction.')
    ITALIAN = @('Cerca','PNG tracciati','Preferiti','Recenti','Impostazioni','Cerca e seleziona','Nome o ID del PNG','Tastiera virtuale','Usa bersaglio console','Usa bersaglio mirino','Cancella selezione','Forse cercavi:','Filtri e ordinamento','Filtri di testo','Il plugin contiene','La posizione contiene','Stato PNG','Qualsiasi','Sì','No','Vivo','Morto','Abilitato','Disabilitato','Seguace','Potenziale seguace','Caricato','Non caricato','Elenchi e contesto','Solo preferiti','Solo tracciati','Stessa posizione','Includi PNG generici','Ordine','Ordina per','Crescente','Cancella filtri','Nome','Plugin','Livello','Posizione','Distanza','PNG selezionato','Nessun PNG selezionato.','Comandi','Copia ID','FormID','EditorID','Stabile','Dettagli PNG','Identità','Stato','Selezione bersaglio','Seleziona automaticamente bersaglio console','Seleziona automaticamente bersaglio mirino','Visualizzazione e ricerca','Layout tastiera virtuale','Alfabetico','Unità di distanza','Metri','Piedi','Unità di gioco','Dettaglio risultati','Dettagliato','Compatto','Tracciamento','Avvisa quando muore un PNG tracciato','Manutenzione','Aggiorna indice di ricerca','Ricrea dati di ricerca','Prepara per la disinstallazione','Casuale','Direzione','Decrescente','Non usata','L''ordine casuale non usa una direzione.')
    GERMAN = @('Suche','Verfolgte NPCs','Favoriten','Zuletzt','Einstellungen','Suchen und auswählen','NPC-Name oder ID','Virtuelle Tastatur','Konsolenziel verwenden','Fadenkreuzziel verwenden','Auswahl löschen','Meintest du:','Filter und Sortierung','Textfilter','Plugin enthält','Ort enthält','NPC-Status','Beliebig','Ja','Nein','Lebendig','Tot','Aktiviert','Deaktiviert','Begleiter','Möglicher Begleiter','Geladen','Nicht geladen','Listen und Kontext','Nur Favoriten','Nur verfolgte','Gleicher Ort','Generische NPCs einschließen','Reihenfolge','Sortieren nach','Aufsteigend','Filter löschen','Name','Plugin','Stufe','Ort','Entfernung','Ausgewählter NPC','Kein NPC ausgewählt.','Befehle','ID kopieren','FormID','EditorID','Stabil','NPC-Details','Identität','Status','Zielauswahl','Konsolenziel automatisch auswählen','Fadenkreuzziel automatisch auswählen','Anzeige und Suche','Layout der virtuellen Tastatur','Alphabetisch','Entfernungseinheit','Meter','Fuß','Spieleinheiten','Ergebnisdetails','Detailliert','Kompakt','Verfolgung','Bei Tod eines verfolgten NPC benachrichtigen','Wartung','Suchindex aktualisieren','Suchdaten neu erstellen','Für Deinstallation vorbereiten','Zufällig','Richtung','Absteigend','Nicht verwendet','Die zufällige Reihenfolge verwendet keine Richtung.')
    SPANISH = @('Buscar','NPC rastreados','Favoritos','Recientes','Ajustes','Buscar y seleccionar','Nombre o ID del NPC','Teclado virtual','Usar objetivo de consola','Usar objetivo de la mira','Borrar selección','Quizá quisiste decir:','Filtros y orden','Filtros de texto','El plugin contiene','La ubicación contiene','Estado del NPC','Cualquiera','Sí','No','Vivo','Muerto','Activado','Desactivado','Seguidor','Seguidor potencial','Cargado','No cargado','Listas y contexto','Solo favoritos','Solo rastreados','Misma ubicación','Incluir NPC genéricos','Orden','Ordenar por','Ascendente','Borrar filtros','Nombre','Plugin','Nivel','Ubicación','Distancia','NPC seleccionado','Ningún NPC seleccionado.','Comandos','Copiar ID','FormID','EditorID','Estable','Detalles del NPC','Identidad','Estado','Selección de objetivo','Seleccionar objetivo de consola automáticamente','Seleccionar objetivo de mira automáticamente','Pantalla y búsqueda','Diseño del teclado virtual','Alfabético','Unidad de distancia','Metros','Pies','Unidades del juego','Detalle de resultados','Detallado','Compacto','Rastreo','Avisar cuando muera un NPC rastreado','Mantenimiento','Actualizar índice de búsqueda','Reconstruir datos de búsqueda','Preparar para desinstalar','Aleatorio','Dirección','Descendente','No se usa','El orden aleatorio no usa dirección.')
    POLISH = @('Szukaj','Śledzeni NPC','Ulubione','Ostatnie','Ustawienia','Szukaj i wybierz','Nazwa lub ID NPC','Klawiatura ekranowa','Użyj celu z konsoli','Użyj celu pod celownikiem','Wyczyść wybór','Czy chodziło o:','Filtry i sortowanie','Filtry tekstu','Wtyczka zawiera','Lokacja zawiera','Stan NPC','Dowolny','Tak','Nie','Żywy','Martwy','Włączony','Wyłączony','Towarzysz','Potencjalny towarzysz','Wczytany','Niewczytany','Listy i kontekst','Tylko ulubione','Tylko śledzeni','Ta sama lokacja','Uwzględnij zwykłych NPC','Kolejność','Sortuj według','Rosnąco','Wyczyść filtry','Nazwa','Wtyczka','Poziom','Lokacja','Odległość','Wybrany NPC','Nie wybrano NPC.','Polecenia','Kopiuj ID','FormID','EditorID','Stabilny','Szczegóły NPC','Tożsamość','Stan','Wybór celu','Automatycznie wybierz cel z konsoli','Automatycznie wybierz cel pod celownikiem','Wyświetlanie i wyszukiwanie','Układ klawiatury ekranowej','Alfabetyczny','Jednostka odległości','Metry','Stopy','Jednostki gry','Szczegóły wyników','Szczegółowy','Kompaktowy','Śledzenie','Powiadom o śmierci śledzonego NPC','Konserwacja','Odśwież indeks wyszukiwania','Odbuduj dane wyszukiwania','Przygotuj do odinstalowania','Losowo','Kierunek','Malejąco','Nieużywany','Kolejność losowa nie używa kierunku.')
    RUSSIAN = @('Поиск','Отслеживаемые НИП','Избранное','Недавние','Настройки','Поиск и выбор','Имя или ID НИП','Виртуальная клавиатура','Использовать цель консоли','Использовать цель прицела','Очистить выбор','Возможно, вы имели в виду:','Фильтры и сортировка','Текстовые фильтры','Плагин содержит','Местоположение содержит','Состояние НИП','Любое','Да','Нет','Жив','Мёртв','Включён','Отключён','Спутник','Возможный спутник','Загружен','Не загружен','Списки и контекст','Только избранные','Только отслеживаемые','То же место','Включить обычных НИП','Порядок','Сортировать по','По возрастанию','Очистить фильтры','Имя','Плагин','Уровень','Местоположение','Расстояние','Выбранный НИП','НИП не выбран.','Команды','Копировать ID','FormID','EditorID','Стабильный','Сведения о НИП','Идентификаторы','Состояние','Выбор цели','Автовыбор цели консоли','Автовыбор цели прицела','Отображение и поиск','Раскладка виртуальной клавиатуры','Алфавитная','Единица расстояния','Метры','Футы','Игровые единицы','Подробность результатов','Подробно','Компактно','Отслеживание','Сообщать о смерти отслеживаемого НИП','Обслуживание','Обновить индекс поиска','Перестроить данные поиска','Подготовить к удалению','Случайно','Направление','По убыванию','Не используется','Случайный порядок не использует направление.')
    CHINESE = @('搜索','已追踪 NPC','收藏','最近','设置','搜索并选择','NPC 名称或 ID','虚拟键盘','使用控制台目标','使用准星目标','清除选择','你是否想找：','筛选与排序','文本筛选','插件包含','位置包含','NPC 状态','任意','是','否','存活','死亡','已启用','已禁用','随从','潜在随从','已加载','未加载','列表与上下文','仅收藏','仅追踪','相同位置','包含通用 NPC','顺序','排序方式','升序','清除筛选','名称','插件','等级','位置','距离','已选 NPC','未选择 NPC。','命令','复制 ID','FormID','EditorID','稳定 ID','NPC 详情','身份','状态','目标选择','自动选择控制台目标','自动选择准星目标','显示与搜索','虚拟键盘布局','字母顺序','距离单位','米','英尺','游戏单位','结果详情','详细','紧凑','追踪','追踪的 NPC 死亡时通知','维护','刷新搜索索引','重建搜索数据','准备卸载','随机','方向','降序','不使用','随机排序不使用方向。')
    JAPANESE = @('検索','追跡中のNPC','お気に入り','最近','設定','検索と選択','NPC名またはID','仮想キーボード','コンソール対象を使用','照準対象を使用','選択を解除','候補：','フィルターと並べ替え','文字フィルター','プラグイン名を含む','場所を含む','NPC状態','すべて','はい','いいえ','生存','死亡','有効','無効','フォロワー','フォロワー候補','ロード済み','未ロード','一覧と条件','お気に入りのみ','追跡中のみ','同じ場所','一般NPCを含める','順序','並べ替え','昇順','フィルターを解除','名前','プラグイン','レベル','場所','距離','選択中のNPC','NPCが選択されていません。','コマンド','IDをコピー','FormID','EditorID','安定ID','NPC詳細','識別情報','状態','対象選択','コンソール対象を自動選択','照準対象を自動選択','表示と検索','仮想キーボード配列','アルファベット順','距離単位','メートル','フィート','ゲーム単位','結果の詳細','詳細','コンパクト','追跡','追跡中NPCの死亡を通知','メンテナンス','検索索引を更新','検索データを再構築','アンインストールの準備','ランダム','方向','降順','使用しない','ランダム順では方向を使用しません。')
}
$locationPageTranslations = @{
    FRENCH = 'Lieux'
    ITALIAN = 'Località'
    GERMAN = 'Orte'
    SPANISH = 'Ubicaciones'
    POLISH = 'Lokacje'
    RUSSIAN = 'Локации'
    CHINESE = '地点'
    JAPANESE = '場所'
}

$primaryKeyNames = @(
    'SearchPage','TrackedPage','FavoritesPage','RecentPage','SettingsPage','SearchHeading','SearchInput',
    'VirtualKeyboard','UseConsoleTarget','UseCrosshairTarget','ClearSelection','DidYouMean',
    'FiltersAndSorting','TextFilters','PluginContains','LocationContains','NpcStatus','Any','Yes','No',
    'Alive','Dead','Enabled','Disabled','Follower','PotentialFollower','Loaded','Unloaded','ListsAndContext',
    'FavoritesOnly','TrackedOnly','SameLocation','IncludeGeneric','Order','SortBy','Ascending','ClearFilters',
    'Name','Plugin','Level','Location','Distance','SelectedNpc','NoNpcSelected','Commands','CopyId','FormId',
    'EditorId','Stable','NpcDetails','Identity','Status','TargetSelection','AutoConsole','AutoCrosshair',
    'DisplayAndSearch','KeyboardLayout','Alphabetical','DistanceUnit','Meters','Feet','GameUnits','ResultDetail',
  'Detailed','Compact','Tracking','NotifyDeath','Maintenance','RefreshIndex','RebuildSearchData','PrepareUninstall','Random',
  'Direction','Descending','NotUsed','RandomDirectionNote'
)
$entriesByKey = @{}
foreach ($entry in $entries) { $entriesByKey[$entry.Key] = $entry }
$primaryKeys = foreach ($name in $primaryKeyNames) {
    $key = '$Whereabouts_' + $name
    if (-not $entriesByKey.ContainsKey($key)) { throw "Primary translation key missing: $key" }
    $entriesByKey[$key]
}
foreach ($language in @('ENGLISH','FRENCH','ITALIAN','GERMAN','SPANISH','POLISH','CHINESE','RUSSIAN','JAPANESE')) {
    $lines = foreach ($entry in $entries) { "$($entry.Key)`t$($entry.English)" }
    $runtimePath = Join-Path $ProjectRoot "Interface/Translations/Whereabouts_$language.txt"
    New-Item -ItemType Directory -Path (Split-Path -Parent $runtimePath) -Force | Out-Null
    [System.IO.File]::WriteAllText($runtimePath, (($lines -join "`r`n") + "`r`n"), [System.Text.UnicodeEncoding]::new($false, $true))
    if ($language -eq 'ENGLISH') { continue }
    $translated = $common[$language]
    if ($translated.Count -ne $primaryKeys.Count) {
        throw "$language translation count mismatch: $($translated.Count) values for $($primaryKeys.Count) keys"
    }
    $map = @{}
    for ($index = 0; $index -lt $primaryKeys.Count; ++$index) { $map[$primaryKeys[$index].Key] = $translated[$index] }
    $map['$Whereabouts_LocationsPage'] = $locationPageTranslations[$language]
    $machinePath = Join-Path $ProjectRoot "translations/machine/Whereabouts_$language.txt"
    if (Test-Path -LiteralPath $machinePath) {
        foreach ($existingLine in [System.IO.File]::ReadAllLines($machinePath)) {
            $parts = $existingLine.Split([char]9, 2)
            if ($parts.Count -ne 2 -or [string]::IsNullOrWhiteSpace($parts[1])) {
                throw "Malformed existing translation in $machinePath"
            }
            $map[$parts[0]] = $parts[1]
        }
    }
    $machineLines = foreach ($entry in $entries) {
        $value = if ($map.ContainsKey($entry.Key)) { $map[$entry.Key] } else { $entry.English }
        "$($entry.Key)`t$value"
    }
    New-Item -ItemType Directory -Path (Split-Path -Parent $machinePath) -Force | Out-Null
    [System.IO.File]::WriteAllText($machinePath, (($machineLines -join "`r`n") + "`r`n"), [System.Text.UnicodeEncoding]::new($false, $true))
}

Write-Output "Generated $($entries.Count) keys for 9 fallback and 8 optional translation files."
