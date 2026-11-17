/** @file settings.h
 * @brief Bounded UI settings and atomic persistence. Single owner thread.
 * No secrets are stored: credential_ref names an existing environment variable.
 * All caller objects are fixed-size; functions never retain their pointers.
 */
#ifndef COG_TUI_SETTINGS_H
#define COG_TUI_SETTINGS_H
#include <cog/result.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define COG_TUI_VALUE_CAPACITY 1024u
/** @brief One selected real provider configuration; all arrays NUL-terminated. */
typedef struct cog_tui_settings {
    char provider[32]; /**< Exactly openrouter; mock is a runtime test option. */
    char model[COG_TUI_VALUE_CAPACITY]; /**< Nonempty model identifier. */
    char base_url[COG_TUI_VALUE_CAPACITY]; /**< HTTPS URL without credentials. */
    char credential_ref[COG_TUI_VALUE_CAPACITY]; /**< env:NAME; no raw secret. */
} cog_tui_settings_t;
/** @brief Fill defaults; model is empty and needs user selection.
 * @param[out] settings Required destination.
 * @return OK or NULL_PTR. No allocation or IO.
 */
cog_result_t cog_tui_settings_defaults(cog_tui_settings_t *settings);
/** @brief Check complete, bounded settings without resolving credentials/network.
 * @param[in] settings Required input.
 * @return OK; NULL_PTR; INVALID_ARG for missing/malformed fields; UNSUPPORTED
 * for another provider. Reject unterminated arrays and control characters.
 */
cog_result_t cog_tui_settings_validate(const cog_tui_settings_t *settings);
/** @brief Load one complete settings file, replacing output only on success.
 * @param[in] path Nonempty file path; parent directory already exists.
 * @param[out] settings Required destination.
 * @return OK; NULL_PTR; INVALID_ARG; NOT_FOUND; IO; DENIED for symlink/nonregular
 * or file not owned by effective UID or accessible to group/other; LIMIT for
 * oversized input; PROTOCOL for unknown/duplicate/missing keys or malformed data.
 * Credential references match env:[A-Za-z_][A-Za-z0-9_]*. HTTPS URLs require
 * a host and forbid userinfo, query and fragment.
 * @note No directory creation. File format is version=1 plus provider, model,
 * base_url and credential_ref key=value lines; reject embedded NUL/CR, multiline
 * values and any extra keys. Reads are bounded to 8192 bytes.
 */
cog_result_t cog_tui_settings_load(const char *path, cog_tui_settings_t *settings);
/** @brief Atomically replace settings through a private sibling temporary file.
 * @param[in] path Nonempty destination in an existing user-owned private directory.
 * @param[in] settings Valid complete settings; borrowed only during call.
 * @return OK; NULL_PTR; INVALID_ARG; UNSUPPORTED; DENIED for unsafe target/parent;
 * IO for create/write/fsync/rename failures. Before rename, failures leave old
 * file intact. INDETERMINATE means rename succeeded but parent-directory sync
 * failed: new bytes may already be visible; inspect before retry. The
 * replacement is mode0600 and owned by effective UID.
 * @note Flush file before rename. No secrets or shell expansion. Reject
 * symlink/nonregular existing targets. Parent must be owned by effective UID
 * and not group/other accessible; caller creates its configuration directory.
 */
cog_result_t cog_tui_settings_save(const char *path, const cog_tui_settings_t *settings);
#ifdef __cplusplus
}
#endif
#endif
