#pragma once

#include "Profiler/SimpleProfiler.hpp"
#include "Profiler/SimpleSampler.hpp"

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <vector>
#include <span>
#include <algorithm>

namespace SimpleProfiler::Private
{
	using ChildrenMap = std::vector<std::vector<SimpleProfiler::NodeId>>;
	using SamplerVec = std::vector<Sampler>;
	
	[[nodiscard]] inline double CalcPercentage(const double aPart, const double aTotal)
	{
		return aTotal == 0 ? 0.0 : (aPart * 100.0) / aTotal;
	}

	[[nodiscard]] inline double CalcNodePercentage(const auto& aScopeInfos, const SimpleProfiler::ScopeInfo& aScopeInfo)
	{
		return aScopeInfo.mParentNodeId == SimpleProfiler::gNullNode ? 0.0 : CalcPercentage(aScopeInfo.mTimeUs, aScopeInfos[aScopeInfo.mParentNodeId].mTimeUs);
	}

	[[nodiscard]] inline int CalcDelta(const SamplerVec& aTimeSamplers,
		const SamplerVec& aPercentSamplers,
		const SimpleProfiler::ScopeInfo& aInfoA,
		const SimpleProfiler::ScopeInfo& aInfoB,
		const unsigned int aColumnUserID)
	{
		if (aColumnUserID == 0u) // Sort by label
			return aInfoA.mLabebl.compare(aInfoB.mLabebl);

		if (aColumnUserID == 1u) // Sort by time
		{
			const double timeA = aTimeSamplers[aInfoA.mNodeId].GetAverage();
			const double timeB = aTimeSamplers[aInfoB.mNodeId].GetAverage();

			return (timeA > timeB) ? 1 : (timeA < timeB) ? -1 : 0;
		}

		if (aColumnUserID == 2u) // Sort by percent
		{
			const double percentA = aPercentSamplers[aInfoA.mNodeId].GetAverage();
			const double percentB = aPercentSamplers[aInfoB.mNodeId].GetAverage();

			return (percentA > percentB) ? 1 : (percentA < percentB) ? -1 : 0;
		}

		if (aColumnUserID == 3u) // Sort by location
		{
			// Compare by file name first, then by line number
			const int delta = aInfoA.mFile.compare(aInfoB.mFile);
			return delta == 0 ? aInfoA.mLine - aInfoB.mLine : delta;
		}

		return 0;
	}

	inline void RenderNode(const SamplerVec& timeSamplers,
		const SamplerVec& percentSamplers,
		SimpleProfiler::NodeId nodeId,
		const std::span<const SimpleProfiler::ScopeInfo>& allNodes,
		const ChildrenMap& childrenMap)
	{
		const SimpleProfiler::ScopeInfo& info = allNodes[nodeId];
		const std::vector<SimpleProfiler::NodeId>& children = childrenMap[nodeId];

		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);

		ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
		if (children.empty())
		{
			// Leaf nodes don't need a collapsing arrow and don't push to the ID stack
			nodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		}

		constexpr const char spaces[33] = "                                ";
		const char* spacesPtr = spaces + sizeof(spaces) - 1u - (info.mDepth * 2); // points to the null terminator

		// We use the node's ID as a unique identifier for ImGui
		const bool isNodeOpen = ImGui::TreeNodeEx(reinterpret_cast<void*>(nodeId), nodeFlags, "%s", info.mLabebl.data());

		ImGui::TableSetColumnIndex(1);
		ImGui::Text("%s%.3f", spacesPtr, static_cast<double>(timeSamplers[nodeId].GetAverage())); // Already in ms

		ImGui::TableSetColumnIndex(2);
		if (info.mParentNodeId != SimpleProfiler::gNullNode)
		{

			if (const SimpleProfiler::ScopeInfo& parentInfo = allNodes[info.mParentNodeId]; parentInfo.mTimeUs > 0)
				ImGui::Text("%s%.1f%%", spacesPtr + 1, percentSamplers[nodeId].GetAverage());
			else
				ImGui::Text("%sN/A", spacesPtr + 1);
		}
		else
			ImGui::Text(" "); // Root nodes have no parent

		ImGui::TableSetColumnIndex(3);

		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
		ImGui::Text("%s:%d", info.mFile.substr(info.mFile.rfind('/') + 1u).data(), info.mLine);
		ImGui::PopStyleColor();

		// Recurse into children if the node is open and has children
		if (isNodeOpen && !children.empty())
		{
			for (const SimpleProfiler::NodeId childId : children)
				RenderNode(timeSamplers, percentSamplers, childId, allNodes, childrenMap);

			ImGui::TreePop(); // This is only needed if the node was not a leaf
		}
	}

}

namespace SimpleProfiler
{
	inline void ShowImguiProfiler()
	{
		const std::span<const ScopeInfo> scopeInfos = SimpleProfiler::GetScopeInfos();

		if (scopeInfos.empty())
		{
			ImGui::Text("No profiling data captured for this thread.");
			return;
		}

		static thread_local Private::ChildrenMap childrenMap(SimpleProfiler::gMaxNodes);
		static thread_local std::vector<SimpleProfiler::NodeId> rootNodes;
		static thread_local Private::SamplerVec nodeTimeSamplers(SimpleProfiler::gMaxNodes, Sampler{64u});
		static thread_local Private::SamplerVec nodePercentSamplers(SimpleProfiler::gMaxNodes, Sampler{64u});

		SimpleProfiler::PopulateNodes(scopeInfos, childrenMap, rootNodes); // Clears as the first step

		for (SimpleProfiler::NodeId i = 0; i < static_cast<SimpleProfiler::NodeId>(scopeInfos.size()); ++i)
		{
			nodeTimeSamplers[i].Record(static_cast<float>(scopeInfos[i].mTimeUs) / 1000.0f);
			nodePercentSamplers[i].Record(static_cast<float>(Private::CalcNodePercentage(scopeInfos, scopeInfos[i])));
		}

		if (!ImGui::BeginTable("ProfilerTreeView",
			4,
			ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
			ImGuiTableFlags_Sortable))
			return;

		ImGui::TableSetupColumn("Scope", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort, 120.0f, 0);
		ImGui::TableSetupColumn("Time (ms)", ImGuiTableColumnFlags_WidthStretch, 120.0f, 1);
		ImGui::TableSetupColumn("% of Parent", ImGuiTableColumnFlags_WidthStretch, 80.0f, 2);
		ImGui::TableSetupColumn("Location", ImGuiTableColumnFlags_WidthStretch, 120.0f, 3);

		ImGui::TableHeadersRow();

		if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs())
		{
			const auto nodeComparer = [&](const SimpleProfiler::NodeId a, const SimpleProfiler::NodeId b) -> bool
				{
					const auto& infoA = scopeInfos[a];
					const auto& infoB = scopeInfos[b];

					for (int i = 0; i < specs->SpecsCount; ++i)
					{
						const ImGuiTableColumnSortSpecs* sortSpec = &specs->Specs[i];
						const int delta = Private::CalcDelta(nodeTimeSamplers, nodePercentSamplers, infoA, infoB, sortSpec->ColumnUserID);

						if (delta == 0)
							continue;

						return (sortSpec->SortDirection == ImGuiSortDirection_Ascending) ? (delta < 0) : (delta > 0);
					}

					return false;
				};

			for (auto& vec : childrenMap)
				std::sort(vec.begin(), vec.end(), nodeComparer);

			std::sort(rootNodes.begin(), rootNodes.end(), nodeComparer);
		}

		for (const auto rootId : rootNodes)
			Private::RenderNode(nodeTimeSamplers, nodePercentSamplers, rootId, scopeInfos, childrenMap);

		ImGui::EndTable();
	}
}
