#pragma once

#include "Timer.hpp"

#include <cassert>
#include <span>
#include <string>
#include <vector>

namespace SimpleProfiler
{
	using NodeId = std::size_t;

	inline constexpr NodeId gNullNode = static_cast<NodeId>(-1);
	inline constexpr NodeId gMaxNodes = 128;

	struct [[nodiscard]] ScopeInfo
	{
		std::string_view mLabebl;
		std::string_view mFile;
		std::string_view mFunction;

		int mLine;

		double mTimeUs;

		NodeId mNodeId;
		NodeId mParentNodeId;

		std::size_t mDepth;
	};
}

#ifdef SIMPLE_PROFILER_ENABLED
namespace SimpleProfiler::Private
{
	struct [[nodiscard]] Database
	{
		ScopeInfo mNodes[gMaxNodes]{};

		NodeId mNextNodeId = 0u;
		NodeId mCurrentNodeId = gNullNode;

		std::size_t mCurrentDepth = 0u;
		
		[[nodiscard]] ScopeInfo& InitNode(const std::string_view label,
			const std::string_view file,
			const std::string_view func,
			const int line)
		{
			const NodeId id = mNextNodeId++;
			assert(id < gMaxNodes);

			mNodes[id] = ScopeInfo{
				.mLabebl = label,
				.mFile = file,
				.mFunction = func,
				.mLine = line,
				.mTimeUs = -1,
				.mNodeId = id,
				.mParentNodeId = gNullNode,
				.mDepth = mCurrentDepth,
			};

			return mNodes[id];
		}
	};

	inline thread_local Database gThreadLocalDatabase;

	struct [[nodiscard]] ScopeGuard
	{
		ScopeInfo& scopeInfo;
		Time::Timer mTimer;
		NodeId previousNodeId;
		
		inline explicit ScopeGuard(ScopeInfo& theScopeInfo) :
			scopeInfo{theScopeInfo},
			previousNodeId{Private::gThreadLocalDatabase.mCurrentNodeId}
		{
			mTimer.StartTimer();

			Database& db = Private::gThreadLocalDatabase;

			scopeInfo.mParentNodeId = db.mCurrentNodeId;
			db.mCurrentNodeId = scopeInfo.mNodeId;
			db.mCurrentDepth = scopeInfo.mDepth + 1u;
		}

		inline ~ScopeGuard()
		{
			Database& db = Private::gThreadLocalDatabase;

			mTimer.EndTimer();
			scopeInfo.mTimeUs = mTimer.GetDurationMicroseconds();
			db.mCurrentNodeId = previousNodeId;
			db.mCurrentDepth = scopeInfo.mDepth;
		}
	};
}
#endif

namespace SimpleProfiler
{
	[[nodiscard]] inline std::span<const ScopeInfo> GetScopeInfos()
	{
#ifdef SIMPLE_PROFILER_ENABLED
		return std::span<const ScopeInfo>{Private::gThreadLocalDatabase.mNodes, Private::gThreadLocalDatabase.mNextNodeId};
#else
		return {};
#endif
	}

	inline void PopulateNodes([[maybe_unused]] std::span<const ScopeInfo> scopeInfos,
		[[maybe_unused]] std::vector <std::vector<NodeId>>& childrenMap,
		[[maybe_unused]] std::vector<NodeId>& rootNodes)
	{
#ifdef SIMPLE_PROFILER_ENABLED
		childrenMap.resize(gMaxNodes);

		for (std::vector<NodeId>& vec : childrenMap)
			vec.clear();

		rootNodes.clear();

		for (const ScopeInfo& info : scopeInfos)
		{
			if (info.mTimeUs == -1)
				continue;

			if (info.mParentNodeId == gNullNode) // top-level node
				rootNodes.push_back(info.mNodeId);
			else // child node
				childrenMap[info.mParentNodeId].push_back(info.mNodeId);
		}
#endif
	}

	inline void ResetNodes()
	{
#ifdef SIMPLE_PROFILER_ENABLED
		for (ScopeInfo& node : Private::gThreadLocalDatabase.mNodes)
			node.mTimeUs = -1;
#endif
	}
}

#define SIMPLE_PROFILER_PRIVATE_CONCAT_TOKENS_IMPL(a, b) a##b

#define SIMPLE_PROFILER_PRIVATE_CONCAT_TOKENS(a, b) SIMPLE_PROFILER_PRIVATE_CONCAT_TOKENS_IMPL(a, b)

#define SIMPLE_PROFILER_PRIVATE_UNIQUE_NAME(name) SIMPLE_PROFILER_PRIVATE_CONCAT_TOKENS(name, __LINE__)

#ifdef SIMPLE_PROFILER_ENABLED

#define SIMPLE_PROFILER_PROFILE_SCOPE(label) \
\
static thread_local auto& SIMPLE_PROFILER_PRIVATE_UNIQUE_NAME( \
    sfProfilerScopeInfo) = ::SimpleProfiler::Private::gThreadLocalDatabase.InitNode((label), __FILE__, __func__, __LINE__); \
 \
const ::SimpleProfiler::Private::ScopeGuard SIMPLE_PROFILER_PRIVATE_UNIQUE_NAME(sfProfilerScopeGuard)( \
SIMPLE_PROFILER_PRIVATE_UNIQUE_NAME(sfProfilerScopeInfo))
#else

#define SIMPLE_PROFILER_PROFILE_SCOPE(label) (void)0

#endif
