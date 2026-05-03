package com.duxman.lednext.ui

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.viewModelScope
import com.duxman.lednext.data.Controller
import com.duxman.lednext.network.StatusChecker
import com.duxman.lednext.repository.ControllerRepository
import kotlinx.coroutines.launch

class MainViewModel(application: Application) : AndroidViewModel(application) {

    private val repository = ControllerRepository(application)

    private val _controllers = MutableLiveData<List<Controller>>(emptyList())
    val controllers: LiveData<List<Controller>> = _controllers

    private val _isRefreshing = MutableLiveData(false)
    val isRefreshing: LiveData<Boolean> = _isRefreshing

    init {
        loadAndRefresh()
    }

    fun loadAndRefresh() {
        viewModelScope.launch {
            _isRefreshing.value = true
            val stored = repository.getAll()
            _controllers.value = stored
            val updated = StatusChecker.checkAll(stored)
            _controllers.value = updated
            _isRefreshing.value = false
        }
    }

    fun addController(name: String, ip: String) {
        val controller = Controller(name = name.trim(), ip = ip.trim())
        repository.add(controller)
        loadAndRefresh()
    }

    fun removeController(id: String) {
        repository.remove(id)
        loadAndRefresh()
    }
}
