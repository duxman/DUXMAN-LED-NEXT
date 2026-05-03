package com.duxman.lednext.repository

import android.content.Context
import com.duxman.lednext.data.Controller
import org.json.JSONArray
import org.json.JSONObject

/**
 * Persists the list of controllers in SharedPreferences as a JSON array.
 */
class ControllerRepository(context: Context) {

    private val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)

    fun getAll(): List<Controller> {
        val raw = prefs.getString(KEY_LIST, "[]") ?: "[]"
        return try {
            val array = JSONArray(raw)
            (0 until array.length()).map { i ->
                val obj = array.getJSONObject(i)
                Controller(
                    id = obj.getString("id"),
                    name = obj.getString("name"),
                    ip = obj.getString("ip")
                )
            }
        } catch (e: Exception) {
            emptyList()
        }
    }

    fun add(controller: Controller) {
        val list = getAll().toMutableList()
        list.add(controller)
        saveAll(list)
    }

    fun remove(id: String) {
        val list = getAll().filter { it.id != id }
        saveAll(list)
    }

    fun update(controller: Controller) {
        val list = getAll().map { if (it.id == controller.id) controller else it }
        saveAll(list)
    }

    private fun saveAll(list: List<Controller>) {
        val array = JSONArray()
        list.forEach { c ->
            array.put(JSONObject().apply {
                put("id", c.id)
                put("name", c.name)
                put("ip", c.ip)
            })
        }
        prefs.edit().putString(KEY_LIST, array.toString()).apply()
    }

    companion object {
        private const val PREFS_NAME = "duxman_controllers"
        private const val KEY_LIST = "list"
    }
}
