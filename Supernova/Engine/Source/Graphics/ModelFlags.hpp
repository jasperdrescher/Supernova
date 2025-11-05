#pragma once

enum class FileLoadingFlags : unsigned int
{
	None = 0,
	PreTransformVertices = 1 << 0,
	PreMultiplyVertexColors = 1 << 1,
	FlipY = 1 << 2,
	DontLoadImages = 1 << 3
};

inline constexpr FileLoadingFlags operator|(FileLoadingFlags a, FileLoadingFlags b)
{
	return static_cast<FileLoadingFlags>(static_cast<unsigned int>(a) | static_cast<unsigned int>(b));
}

inline constexpr FileLoadingFlags operator&(FileLoadingFlags a, FileLoadingFlags b)
{
	return static_cast<FileLoadingFlags>(static_cast<unsigned int>(a) & static_cast<unsigned int>(b));
}

enum class RenderFlags : unsigned int
{
	None = 0,
	BindImages = 1 << 0,
	RenderOpaqueNodes = 1 << 1,
	RenderAlphaMaskedNodes = 1 << 2,
	RenderAlphaBlendedNodes = 1 << 3
};

inline constexpr RenderFlags operator|(RenderFlags a, RenderFlags b)
{
	return static_cast<RenderFlags>(static_cast<unsigned int>(a) | static_cast<unsigned int>(b));
}

inline constexpr RenderFlags operator&(RenderFlags a, RenderFlags b)
{
	return static_cast<RenderFlags>(static_cast<unsigned int>(a) & static_cast<unsigned int>(b));
}
