ALTER TABLE event ADD uri TEXT NOT NULL DEFAULT '';
ALTER TABLE event ADD ip INTEGER UNSIGNED NOT NULL;
ALTER TABLE event ADD cookies TEXT NOT NULL DEFAULT '';