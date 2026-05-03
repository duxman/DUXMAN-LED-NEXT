package com.duxman.lednext.ui

import android.content.Intent
import android.os.Bundle
import android.view.Menu
import android.view.MenuItem
import android.view.View
import androidx.activity.viewModels
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.recyclerview.widget.LinearLayoutManager
import com.duxman.lednext.R
import com.duxman.lednext.data.Controller
import com.duxman.lednext.databinding.ActivityMainBinding
import com.duxman.lednext.databinding.DialogAddControllerBinding

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding
    private val viewModel: MainViewModel by viewModels()
    private lateinit var adapter: ControllerAdapter

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)
        setSupportActionBar(binding.toolbar)

        adapter = ControllerAdapter(
            onOpen = { controller -> openWebView(controller) },
            onDelete = { controller -> confirmDelete(controller) }
        )

        binding.recyclerView.apply {
            layoutManager = LinearLayoutManager(this@MainActivity)
            adapter = this@MainActivity.adapter
        }

        binding.swipeRefresh.setOnRefreshListener { viewModel.loadAndRefresh() }

        binding.fabAdd.setOnClickListener { showAddControllerDialog() }

        viewModel.controllers.observe(this) { list ->
            adapter.submitList(list)
            binding.emptyView.visibility = if (list.isEmpty()) View.VISIBLE else View.GONE
        }

        viewModel.isRefreshing.observe(this) { refreshing ->
            binding.swipeRefresh.isRefreshing = refreshing
        }
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        menuInflater.inflate(R.menu.menu_main, menu)
        return true
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return when (item.itemId) {
            R.id.action_refresh -> {
                viewModel.loadAndRefresh()
                true
            }
            else -> super.onOptionsItemSelected(item)
        }
    }

    private fun openWebView(controller: Controller) {
        val intent = Intent(this, WebViewActivity::class.java).apply {
            putExtra(WebViewActivity.EXTRA_URL, controller.homeUrl)
            putExtra(WebViewActivity.EXTRA_TITLE, controller.name)
        }
        startActivity(intent)
    }

    private fun confirmDelete(controller: Controller) {
        AlertDialog.Builder(this)
            .setTitle(R.string.dialog_delete_title)
            .setMessage(getString(R.string.dialog_delete_message, controller.name))
            .setPositiveButton(R.string.action_delete) { _, _ ->
                viewModel.removeController(controller.id)
            }
            .setNegativeButton(R.string.action_cancel, null)
            .show()
    }

    private fun showAddControllerDialog() {
        val dialogBinding = DialogAddControllerBinding.inflate(layoutInflater)

        AlertDialog.Builder(this)
            .setTitle(R.string.dialog_add_title)
            .setView(dialogBinding.root)
            .setPositiveButton(R.string.action_add) { _, _ ->
                val name = dialogBinding.editName.text?.toString().orEmpty().trim()
                val ip = dialogBinding.editIp.text?.toString().orEmpty().trim()
                if (name.isNotEmpty() && ip.isNotEmpty()) {
                    viewModel.addController(name, ip)
                }
            }
            .setNegativeButton(R.string.action_cancel, null)
            .show()
    }
}
