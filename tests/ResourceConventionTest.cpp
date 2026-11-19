#include <gtest/gtest.h>

#include <stdexcept>

#include "renders/generic/Buffer.h"
#include "renders/generic/Shader.h"

using namespace bird;

TEST(ShaderBuilder, CacheKeyJoinsStagesInOrder) {
	ShaderBuilder builder;
	builder.attachShaderFile("a.vert", VERTEX)
		.attachShaderFile("b.frag", FRAGMENT);
	EXPECT_EQ(builder.cacheKey(), "a.vert|b.frag|");
}

TEST(ShaderBuilder, CacheKeyDistinguishesStageOrder) {
	ShaderBuilder forward;
	forward.attachShaderFile("a.vert", VERTEX)
		.attachShaderFile("b.frag", FRAGMENT);
	ShaderBuilder reversed;
	reversed.attachShaderFile("b.frag", FRAGMENT)
		.attachShaderFile("a.vert", VERTEX);
	EXPECT_NE(forward.cacheKey(), reversed.cacheKey());
}

TEST(ShaderBuilder, BuildWithoutStagesThrows) {
	EXPECT_THROW(ShaderBuilder().build(), std::logic_error);
}

TEST(BufferBuilder, BindingPointRequiresUniformMode) {
	BufferBuilder<float> vertexBuilder(BUFFER_ARRAY | BUFFER_STAGED);
	EXPECT_THROW(vertexBuilder.bindingPoint(0), std::logic_error);
	BufferBuilder<float> uniformBuilder(BUFFER_UNIFORM | BUFFER_STAGED);
	EXPECT_NO_THROW(uniformBuilder.bindingPoint(0));
}
