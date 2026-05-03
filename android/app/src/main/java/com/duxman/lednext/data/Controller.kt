package com.duxman.lednext.data

import java.util.UUID

data class Controller(
    val id: String = UUID.randomUUID().toString(),
    val name: String,
    val ip: String,
    val status: Status = Status.UNKNOWN
) {
    enum class Status { ONLINE, OFFLINE, UNKNOWN }

    /** Sanitizes the stored IP/hostname for safe URL construction. */
    private val safeHost: String
        get() = ip.trim().replace(Regex("[^a-zA-Z0-9.:\\-\\[\\]%]"), "")

    val homeUrl: String get() = "http://$safeHost/"
    val stateApiUrl: String get() = "http://$safeHost/api/v1/state"
}
