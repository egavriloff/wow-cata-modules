-- Optional presentation-only update for an already installed teleporter.
-- Apply manually to the world database, then restart worldserver.
SET NAMES utf8mb4;
-- Module-owned greeting used by the scripted menu (not a database menu binding).
INSERT INTO `npc_text` (`ID`, `text0_0`, `text0_1`, `Probability0`, `BroadcastTextID0`, `lang0`)
VALUES (900100, 'Куда отправимся, $N? Выберите раздел, а затем место назначения.', 'Куда отправимся, $N? Выберите раздел, а затем место назначения.', 1, 0, 0)
ON DUPLICATE KEY UPDATE
    `text0_0` = 'Куда отправимся, $N? Выберите раздел, а затем место назначения.',
    `text0_1` = 'Куда отправимся, $N? Выберите раздел, а затем место назначения.',
    `Probability0` = 1, `BroadcastTextID0` = 0, `lang0` = 0;
