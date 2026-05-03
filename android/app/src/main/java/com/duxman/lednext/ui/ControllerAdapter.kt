package com.duxman.lednext.ui

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import com.duxman.lednext.R
import com.duxman.lednext.data.Controller
import com.duxman.lednext.databinding.ItemControllerBinding

class ControllerAdapter(
    private val onOpen: (Controller) -> Unit,
    private val onDelete: (Controller) -> Unit
) : ListAdapter<Controller, ControllerAdapter.ViewHolder>(DIFF_CALLBACK) {

    inner class ViewHolder(private val binding: ItemControllerBinding) :
        RecyclerView.ViewHolder(binding.root) {

        fun bind(controller: Controller) {
            binding.textName.text = controller.name
            binding.textIp.text = controller.ip

            val (statusText, statusColor) = when (controller.status) {
                Controller.Status.ONLINE -> Pair(
                    binding.root.context.getString(R.string.status_online),
                    R.color.status_online
                )
                Controller.Status.OFFLINE -> Pair(
                    binding.root.context.getString(R.string.status_offline),
                    R.color.status_offline
                )
                Controller.Status.UNKNOWN -> Pair(
                    binding.root.context.getString(R.string.status_unknown),
                    R.color.status_unknown
                )
            }
            binding.textStatus.text = statusText
            binding.textStatus.setTextColor(
                binding.root.context.getColor(statusColor)
            )
            binding.statusDot.setColorFilter(
                binding.root.context.getColor(statusColor)
            )

            binding.root.setOnClickListener { onOpen(controller) }
            binding.buttonDelete.setOnClickListener { onDelete(controller) }
        }
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
        val binding = ItemControllerBinding.inflate(
            LayoutInflater.from(parent.context), parent, false
        )
        return ViewHolder(binding)
    }

    override fun onBindViewHolder(holder: ViewHolder, position: Int) {
        holder.bind(getItem(position))
    }

    companion object {
        private val DIFF_CALLBACK = object : DiffUtil.ItemCallback<Controller>() {
            override fun areItemsTheSame(old: Controller, new: Controller) = old.id == new.id
            override fun areContentsTheSame(old: Controller, new: Controller) = old == new
        }
    }
}
