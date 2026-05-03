package com.duxman.lednext.network

import com.duxman.lednext.data.Controller
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.awaitAll
import kotlinx.coroutines.withContext
import java.net.HttpURLConnection
import java.net.URL

/**
 * Checks whether a controller is reachable by issuing a GET to its /api/v1/state endpoint.
 * A 200 response means ONLINE; any error or non-200 means OFFLINE.
 */
object StatusChecker {

    private const val CONNECT_TIMEOUT_MS = 3_000
    private const val READ_TIMEOUT_MS = 3_000

    suspend fun check(controller: Controller): Controller.Status =
        withContext(Dispatchers.IO) {
            try {
                val url = URL(controller.stateApiUrl)
                val connection = url.openConnection() as HttpURLConnection
                connection.connectTimeout = CONNECT_TIMEOUT_MS
                connection.readTimeout = READ_TIMEOUT_MS
                connection.requestMethod = "GET"
                connection.connect()
                val code = connection.responseCode
                connection.disconnect()
                if (code == HttpURLConnection.HTTP_OK) Controller.Status.ONLINE
                else Controller.Status.OFFLINE
            } catch (e: Exception) {
                Controller.Status.OFFLINE
            }
        }

    suspend fun checkAll(controllers: List<Controller>): List<Controller> =
        withContext(Dispatchers.IO) {
            controllers.map { controller ->
                async { controller.copy(status = check(controller)) }
            }.awaitAll()
        }
}
