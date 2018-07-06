#pragma once
#include "stdafx.h"

wstr get_full_path(crwstr path);
void extract_archive(crwstr archive_path, crwstr archive_root_path, crwstr target_path);
void delete_directory(crwstr path);
void copy_directory(crwstr source_path, crwstr target_path);
void move_directory(crwstr source_path, crwstr target_path);
