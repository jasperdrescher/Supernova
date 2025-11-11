#pragma once

#include "Core/BitmaskOperators.hpp"

enum class FileLoadingFlags : unsigned int
{
	None = 0,
	PreTransformVertices = 1 << 0,
	PreMultiplyVertexColors = 1 << 1,
	FlipY = 1 << 2,
	DontLoadImages = 1 << 3
};

template<>
struct BitmaskOperators<FileLoadingFlags>
{
	static constexpr bool mIsEnabled = true;
};

enum class RenderFlags : unsigned int
{
	None = 0,
	BindImages = 1 << 0,
	RenderOpaqueNodes = 1 << 1,
	RenderAlphaMaskedNodes = 1 << 2,
	RenderAlphaBlendedNodes = 1 << 3
};

template<>
struct BitmaskOperators<RenderFlags>
{
	static constexpr bool mIsEnabled = true;
};

enum DescriptorBindingFlags
{
	ImageBaseColor = 0,
	ImageNormalMap = 1 << 1
};

template<>
struct BitmaskOperators<DescriptorBindingFlags>
{
	static constexpr bool mIsEnabled = true;
};
