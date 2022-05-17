#include "frastVk/utils/eigen.h"

#include "core/app.h"
#include "core/imgui_app.h"

#include "clipmap1/clipmap1.h"

#include <sys/types.h>
#include <iostream>
#include <sys/stat.h>
#include <fcntl.h>

#include <fmt/ostream.h>


// #include "tiled_renderer/tiled_renderer.h"
// using namespace tr1;
#include "tiled_renderer2/tiled_renderer.h"
using namespace tr2;

#include "extra/particleCloud/particleCloud.h"
#include "extra/frustum/frustum.h"
#include "extra/primitives/earthEllipsoid.h"
#include "extra/primitives/ellipsoid.h"
#include "extra/text/textSet.h"

struct GlobeApp : public ImguiApp {

		std::shared_ptr<ClipMapRenderer1> clipmap;
		std::shared_ptr<TiledRenderer> tiledRenderer;
		std::shared_ptr<ParticleCloudRenderer> particleCloud;
		std::shared_ptr<FrustumSet> frustumSet;
		std::shared_ptr<EarthEllipsoid> earthEllipsoid;
		std::shared_ptr<EllipsoidSet> ellpSet;
		std::shared_ptr<SimpleTextSet> textSet;

		inline virtual bool handleKey(int key, int scancode, int action, int mods) {
			if (key == GLFW_KEY_M and action == GLFW_PRESS) showMenu = !showMenu;
			return ImguiApp::handleKey(key,scancode,action,mods);
		}



	inline virtual void initVk() override {
		ImguiApp::initVk();
		initFinalImage();
		//set position of camera offset by loaded mld ctr

		CameraSpec spec { (float)windowWidth, (float)windowHeight, 25 * 3.141 / 180. };


		if (0) {
			camera = std::make_shared<FlatEarthMovingCamera>(spec);
			alignas(16) double pos0[] { 
				(double)(-8590834.045999 / 20037508.342789248),
				(float)(4757669.951554 / 20037508.342789248),
				(double)(2.0 * 1./(1<<(7-1))) };
			alignas(16) double R0[] {
				1,0,0,
				0,-1,0,
				0,0,-1 };
			camera->setPosition(pos0);
			camera->setRotMatrix(R0);
		} else {
			camera = std::make_shared<SphericalEarthMovingCamera>(spec);
			// Eigen::Vector3d pos0 { 0,-2.0,0};
			Eigen::Vector3d pos0 { .2,-1.0,.84};
			Eigen::Matrix<double,3,3,Eigen::RowMajor> R0;
			R0.row(2) = -pos0.normalized();
			R0.row(0) =  R0.row(2).cross(Eigen::Vector3d::UnitZ()).normalized();
			R0.row(1) =  R0.row(2).cross(R0.row(0)).normalized();
			R0.transposeInPlace();
			std::cout << " - R0:\n" << R0 << "\n";
			camera->setPosition(pos0.data());
			camera->setRotMatrix(R0.data());
		}

		ioUsers.push_back(camera.get());
		renderState.camera = camera.get();

		//clipmap = std::make_shared<ClipMapRenderer1>(this);
		//clipmap->init();

		// TiledRendererCfg cfg ("/data/naip/ok/ok16.ft", "/data/elevation/gmted/gmted.ft");
		// TiledRendererCfg cfg ("/data/naip/ok/ok16.ft", "/data/elevation/srtm/usa.11.ft");
		// TiledRendererCfg cfg ("/data/khop/whole.ft", "/data/elevation/srtm/usa.11.ft");
		TiledRendererCfg cfg ("/data/naip/mocoNaip/out.ft", "/data/elevation/gmted/gmted.ft");
		tiledRenderer = std::make_shared<TiledRenderer>(cfg, this);
		tiledRenderer->init();

		particleCloud = std::make_shared<ParticleCloudRenderer>(this, 1024*64);
		std::vector<float> particles4;
		Vector3d p { camera->viewInv()[0*4+3], camera->viewInv()[1*4+3], camera->viewInv()[2*4+3] };
		Vector3d t = p.normalized();
		p = p * .003 + t * .997;
		RowMatrix3d P = RowMatrix3d::Identity() - t*t.transpose();
		const int N = 1024*64;
		for (int i=0; i<N; i++) {
			Vector3d x = p + P * Vector3d::Random() * .03;
			particles4.push_back((float)x[0]);
			particles4.push_back((float)x[1]);
			particles4.push_back((float)x[2]);
			particles4.push_back(((float)i)/N);
		}
		particleCloud->uploadParticles(particles4);

		if (1) {
			frustumSet = std::make_shared<FrustumSet>(this, 2);
			Vector3d pos { 0.17287, -0.754957,  0.631011 };
			Vector3d t = p.normalized();

			Eigen::Matrix<double,3,3,Eigen::RowMajor> R;
			R.row(2) = -pos.normalized();
			R.row(0) =  R.row(2).cross(Eigen::Vector3d::UnitZ()).normalized();
			R.row(1) =  R.row(2).cross(R.row(0)).normalized();
			R.transposeInPlace();

			Vector3d pos2 = pos + Vector3d{1e-5,1e-6,1e-7};
			Eigen::Vector4f color1 { 0,1,0,1 };
			Eigen::Vector4f color2 { 0,0,1,.5 };
			frustumSet->setPose(0, pos, R);
			frustumSet->setIntrin(0, spec.w,spec.h, spec.fx(),spec.fy());
			frustumSet->setColor(0,color1.data());
			frustumSet->setPose(1, pos2, R);
			frustumSet->setIntrin(1, spec.w,spec.h, spec.fx(),spec.fy());
			frustumSet->setColor(1,color2.data());
		}

		earthEllipsoid = std::make_shared<EarthEllipsoid>(this);
		earthEllipsoid->init(0);

		ellpSet = std::make_shared<EllipsoidSet>(this);
		ellpSet->init(mainSubpass());
		RowMatrix4f ellpMatrix (RowMatrix4f::Identity());
		ellpMatrix.topLeftCorner<3,3>() *= 6357000. / (100. * .5);
		// ellpMatrix(2,2) *= 2;
		ellpMatrix.topRightCorner<3,1>() << 0.172367, -0.756938,  0.628275;
		ellpMatrix.topRightCorner<3,1>() = 	-ellpMatrix.topLeftCorner<3,3>().transpose() * ellpMatrix.topRightCorner<3,1>();
		float ellpColor[4] = {1.f,1.f,0.f,1.f};
		ellpSet->set(0,ellpMatrix.data(),ellpColor);

		textSet = std::make_shared<SimpleTextSet>(this);

	}

	inline virtual std::vector<vk::CommandBuffer> doRender(RenderState& rs) override {
		auto& fd = *rs.frameData;

		RowMatrix4f txtMvp = RowMatrix4f::Identity();
		rs.mvpf(txtMvp.data());
		textSet->setAreaAndSize(0.f,0.f, windowWidth, windowHeight, 1.f, txtMvp.data());


		RowMatrix4f txtMatrix;
		txtMatrix.topRightCorner<3,1>() <<  sin(fd.time)*.001 + 0.163815,-0.759765,0.633448;
		txtMatrix.block<3,1>(0,2) = -txtMatrix.topRightCorner<3,1>().normalized();
		txtMatrix.block<3,1>(0,0) = txtMatrix.block<3,1>(0,2).cross(Eigen::Vector3f::UnitZ()).normalized();
		txtMatrix.block<3,1>(0,1) = txtMatrix.block<3,1>(0,2).cross(txtMatrix.block<3,1>(0,0)).normalized();
		txtMatrix.topLeftCorner<3,3>() *= .0005;
		// fmt::print(" - Text Matrix:\n{}\n", txtMatrix);
		txtMatrix.row(3) << 0,0,0,1;
		float color_[] = { 1.f, 1.f, 0.f, .5f};
		textSet->setText(0, "a random number " + std::to_string(rand()%999), txtMatrix.data(), color_);
		txtMatrix.topRightCorner<3,1>() <<  sin(fd.time)*.001 + 0.163815,-0.759765 ,0.633448 - .001;
		textSet->setText(1, "a SeCoNd random number " + std::to_string(rand()%999), txtMatrix.data(), color_);

		// Test caster stuff.
		
		//if (fd.n < 2)
		{
			
			Image casterImage { 256, 256, Image::Format::RGBA };
			casterImage.alloc();
			for (int y=0; y<256; y++) {
				int yy = y + fd.n;
				for (int x=0; x<256; x++) {
					casterImage.buffer[y*256*4+x*4 +0] = (((x/32) + (yy/32))%2 == 0) ? 240 : 130;
					casterImage.buffer[y*256*4+x*4 +1] = (((x/32) + (yy/32))%2 == 0) ? 240 : 130;
					casterImage.buffer[y*256*4+x*4 +2] = (((x/32) + (yy/32))%2 == 0) ? 240 : 130;
					casterImage.buffer[y*256*4+x*4 +3] = 255;
				}
			}

			CasterWaitingData cwd;
			alignas(16) float matrix1[16];
			alignas(16) float matrix2[16];

			if(0) {
				// Use view camera
				for (int i=0;i<16;i++) matrix1[i] = (i%5) == 0;
				rs.mvpf(matrix1);
				cwd.setMask(1u);
				cwd.setMatrix1(matrix1);
			} else {
				// Use a camera inside aoi
				CameraSpec camSpec { (float)256, (float)256, 22 * 3.141 / 180. };
				SphericalEarthMovingCamera tmpCam(camSpec);
				Eigen::Vector3d pos0 {0.171211, -0.756474,  0.630934};
				Eigen::Matrix<double,3,3,Eigen::RowMajor> R0;
				R0.row(2) = -pos0.normalized();
				R0.row(0) =  R0.row(2).cross(Eigen::Vector3d::UnitZ()).normalized();
				R0.row(1) =  R0.row(2).cross(R0.row(0)).normalized();
				R0.transposeInPlace();
				tmpCam.setPosition(pos0.data());
				tmpCam.setRotMatrix(R0.data());
				Eigen::Map<const Eigen::Matrix<double,4,4,Eigen::RowMajor>> view (tmpCam.view());
				Eigen::Map<const Eigen::Matrix<double,4,4,Eigen::RowMajor>> proj (tmpCam.proj());
				Eigen::Matrix<float,4,4,Eigen::RowMajor> m = (proj * view).cast<float>();
				memcpy(matrix1, m.data(), 4*16);
				cwd.setMatrix1(matrix1);

				{
					pos0 = Vector3d {0.173375,  -0.7557, 0.631619};
					tmpCam.setPosition(pos0.data());
					Eigen::Map<const Eigen::Matrix<double,4,4,Eigen::RowMajor>> view (tmpCam.view());
					Eigen::Map<const Eigen::Matrix<double,4,4,Eigen::RowMajor>> proj (tmpCam.proj());
					Eigen::Matrix<float,4,4,Eigen::RowMajor> m = (proj * view).cast<float>();
					memcpy(matrix2, m.data(), 4*16);
					cwd.setMatrix2(matrix2);
				}
				cwd.setMask(3u);
				// cwd.setMask(1u);
			}
			// fmt::print(" - Have caster matrix:\n{}\n", Eigen::Map<const Eigen::Matrix<float,4,4,Eigen::RowMajor>>{matrix1});

			cwd.setImage(casterImage);

			tiledRenderer->setCasterInRenderThread(cwd,this);
		}


		// If we use a frustum, it must be rendered in a pass, so we can use the simpleRenderPass from VkApp.
		vk::CommandBuffer frame_cmd = *fd.cmd;
		// frame_cmd.reset();
		// frame_cmd.begin(vk::CommandBufferBeginInfo{});

		vk::Rect2D aoi { { 0, 0 }, { windowWidth, windowHeight } };
		vk::ClearValue clears_[2] = {
			vk::ClearValue { vk::ClearColorValue { std::array<float,4>{ 0.f,0.05f,.1f,1.f } } }, // color
			vk::ClearValue { vk::ClearColorValue { std::array<float,4>{ 1.f,1.f,1.f,1.f } } }  // depth
		};
		vk::RenderPassBeginInfo rpInfo {
				*simpleRenderPass.pass, *simpleRenderPass.framebuffers[rs.frameData->scIndx],
				aoi, {2, clears_}
		};


		// Note: Can roll back to just one subpass, I thought there was a dependency issue -- it was just a depth-test issue.
		frame_cmd.beginRenderPass(rpInfo, vk::SubpassContents::eInline);
		if (earthEllipsoid) earthEllipsoid->renderInPass(rs, frame_cmd);
		// frame_cmd.nextSubpass(vk::SubpassContents::eInline);

		/*
		std::vector<vk::ImageMemoryBarrier> imgBarriers = {
			vk::ImageMemoryBarrier {
				{},{},
				{}, {},
				{}, {},
				*sc.headlessImages[fd.scIndx],
				vk::ImageSubresourceRange { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
			}};
		if (earthEllipsoid) frame_cmd.pipelineBarrier(
				vk::PipelineStageFlagBits::eAllGraphics,
				vk::PipelineStageFlagBits::eAllGraphics,
				vk::DependencyFlagBits::eDeviceGroup,
				{}, {}, imgBarriers );
				*/

		tiledRenderer->stepAndRenderInPass(renderState, frame_cmd);
		if (frustumSet) frustumSet->renderInPass(rs, frame_cmd);
		if (ellpSet) ellpSet->renderInPass(rs, frame_cmd);
		if (textSet) textSet->render(rs, frame_cmd);
		frame_cmd.endRenderPass();
		// frame_cmd.end();


		std::vector<vk::CommandBuffer> cmds = {
			frame_cmd
		};
		cmds.push_back( particleCloud->render(renderState, *simpleRenderPass.framebuffers[renderState.frameData->scIndx]) );


		/*
		vk::PipelineStageFlags waitMask = vk::PipelineStageFlagBits::eAllGraphics;

		if (headless) {
			if (fd.n <= scNumImages) {
				vk::SubmitInfo submitInfo {
					0, nullptr, // wait sema
						&waitMask,
						(uint32_t)cmds.size(), cmds.data(),
						1, &*fd.renderCompleteSema // signal sema
				};
				queueGfx.submit(submitInfo, *fd.frameDoneFence);
			} else {
				vk::SubmitInfo submitInfo {
					// 0, nullptr, // wait sema
					1, &(*fd.scAcquireSema), // wait sema
						&waitMask,
						//1, &(*commandBuffers[fd.scIndx]),
						(uint32_t)cmds.size(), cmds.data(),
						1, &*fd.renderCompleteSema // signal sema
				};
				queueGfx.submit(submitInfo, *fd.frameDoneFence);
			}
		} else {
			vk::SubmitInfo submitInfo {
				1, &(*fd.scAcquireSema), // wait sema
				&waitMask,
				//1, &(*commandBuffers[fd.scIndx]),
				(uint32_t)cmds.size(), cmds.data(),
				1, &*fd.renderCompleteSema // signal sema
			};
			queueGfx.submit(submitInfo, *fd.frameDoneFence);
		}
		*/
		return cmds;
	}

	inline ~GlobeApp() {
		for (auto& fd : frameDatas)
			deviceGpu.waitForFences({*fd.frameDoneFence}, true, 999999999);
	}

	ResidentImage finalImage;
	void initFinalImage() {
		if (headless) finalImage.createAsCpuVisible(uploader, windowHeight, windowWidth, vk::Format::eR8G8B8A8Uint, nullptr);
	}
	inline virtual void handleCompletedHeadlessRender(RenderState& rs, FrameData& fd) override {
		// if (rand() % 2 == 0) {
		if (false) {
			// Signify that future render calls needn't block on the scAcquireSema semaphore.
			fd.useAcquireSema = false;
			return;
		} else fd.useAcquireSema = true;
		auto& copyCmd = sc.headlessCopyCmds[fd.scIndx];

		/*
    VULKAN_HPP_CONSTEXPR ImageSubresourceLayers( VULKAN_HPP_NAMESPACE::ImageAspectFlags aspectMask_     = {},
                                                 uint32_t                               mipLevel_       = {},
                                                 uint32_t                               baseArrayLayer_ = {},
                                                 uint32_t layerCount_ = {} ) VULKAN_HPP_NOEXCEPT
    VULKAN_HPP_CONSTEXPR ImageCopy( VULKAN_HPP_NAMESPACE::ImageSubresourceLayers srcSubresource_ = {},
                                    VULKAN_HPP_NAMESPACE::Offset3D               srcOffset_      = {},
                                    VULKAN_HPP_NAMESPACE::ImageSubresourceLayers dstSubresource_ = {},
                                    VULKAN_HPP_NAMESPACE::Offset3D               dstOffset_      = {},
                                    VULKAN_HPP_NAMESPACE::Extent3D               extent_ = {} ) VULKAN_HPP_NOEXCEPT
	*/

	vk::ImageCopy region {
		vk::ImageSubresourceLayers { vk::ImageAspectFlagBits::eColor, 0, 0, 1 },
		vk::Offset3D{},
		vk::ImageSubresourceLayers { vk::ImageAspectFlagBits::eColor, 0, 0, 1 },
		vk::Offset3D{},
		vk::Extent3D{windowWidth,windowHeight,1}
	};

		copyCmd.begin({});

		/*
      ImageMemoryBarrier( VULKAN_HPP_NAMESPACE::AccessFlags srcAccessMask_ = {},
                          VULKAN_HPP_NAMESPACE::AccessFlags dstAccessMask_ = {},
                          VULKAN_HPP_NAMESPACE::ImageLayout oldLayout_ = VULKAN_HPP_NAMESPACE::ImageLayout::eUndefined,
                          VULKAN_HPP_NAMESPACE::ImageLayout newLayout_ = VULKAN_HPP_NAMESPACE::ImageLayout::eUndefined,
                          uint32_t                          srcQueueFamilyIndex_        = {},
                          uint32_t                          dstQueueFamilyIndex_        = {},
                          VULKAN_HPP_NAMESPACE::Image       image_                      = {},
                          VULKAN_HPP_NAMESPACE::ImageSubresourceRange subresourceRange_ = {} ) VULKAN_HPP_NOEXCEPT

    VULKAN_HPP_CONSTEXPR ImageSubresourceRange( VULKAN_HPP_NAMESPACE::ImageAspectFlags aspectMask_     = {},
                                                uint32_t                               baseMipLevel_   = {},
                                                uint32_t                               levelCount_     = {},
                                                uint32_t                               baseArrayLayer_ = {},
                                                uint32_t layerCount_ = {} ) VULKAN_HPP_NOEXCEPT
    VULKAN_HPP_INLINE void CommandBuffer::pipelineBarrier(
      VULKAN_HPP_NAMESPACE::PipelineStageFlags                            srcStageMask,
      VULKAN_HPP_NAMESPACE::PipelineStageFlags                            dstStageMask,
      VULKAN_HPP_NAMESPACE::DependencyFlags                               dependencyFlags,
      ArrayProxy<const VULKAN_HPP_NAMESPACE::MemoryBarrier> const &       memoryBarriers,
      ArrayProxy<const VULKAN_HPP_NAMESPACE::BufferMemoryBarrier> const & bufferMemoryBarriers,
      ArrayProxy<const VULKAN_HPP_NAMESPACE::ImageMemoryBarrier> const & imageMemoryBarriers ) const VULKAN_HPP_NOEXCEPT*/
		std::vector<vk::ImageMemoryBarrier> imgBarriers = {
			vk::ImageMemoryBarrier {
				{},{},
				{}, vk::ImageLayout::eTransferSrcOptimal,
				{}, {},
				*sc.headlessImages[fd.scIndx],
				vk::ImageSubresourceRange { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
			},
			vk::ImageMemoryBarrier {
				{},{},
				{}, vk::ImageLayout::eTransferDstOptimal,
				{}, {},
				*finalImage.image,
				vk::ImageSubresourceRange { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
			}
		};
		copyCmd.pipelineBarrier(
				vk::PipelineStageFlagBits::eAllGraphics,
				vk::PipelineStageFlagBits::eAllGraphics,
				vk::DependencyFlagBits::eDeviceGroup,
				{}, {}, imgBarriers);



		copyCmd.copyImage(*sc.headlessImages[fd.scIndx], vk::ImageLayout::eTransferSrcOptimal, *finalImage.image, vk::ImageLayout::eTransferDstOptimal, {1,&region});
		copyCmd.end();

		// if (fd.n == 0) {
		if (true) {
			vk::PipelineStageFlags waitMasks[1] = {vk::PipelineStageFlagBits::eAllGraphics};
			const vk::Semaphore semas[1] = {(*fd.renderCompleteSema)};
			vk::SubmitInfo submitInfo {
				// {1u, &(*fd.renderCompleteSema)}, // wait sema
				{1u, semas}, // wait sema
					{1u, waitMasks},
					{1u, &*copyCmd},
					// {}
					{1u, &*fd.scAcquireSema} // signal sema
			};
			fmt::print(" - submit copyImage wait on {}\n", (void*)&*fd.renderCompleteSema);
			queueGfx.submit(submitInfo, {*sc.headlessCopyDoneFences[fd.scIndx]});

		} else {

			// vk::PipelineStageFlags waitMask = vk::PipelineStageFlagBits::eAllGraphics;
			const vk::Semaphore semas[2] = {(*fd.renderCompleteSema), (*sc.headlessCopyDoneSemas[0])};
			vk::PipelineStageFlags waitMasks[2] = {vk::PipelineStageFlagBits::eAllGraphics,vk::PipelineStageFlagBits::eAllGraphics};
			vk::SubmitInfo submitInfo {
				// {1u, &(*fd.renderCompleteSema)}, // wait sema
				{2u, semas}, // wait sema
					{2u, waitMasks},
					{1u, &*copyCmd},
					{1u, &*fd.scAcquireSema} // signal sema
			};
			queueGfx.submit(submitInfo, {*sc.headlessCopyDoneFences[fd.scIndx]});
		}


		deviceGpu.waitForFences({*sc.headlessCopyDoneFences[fd.scIndx]}, true, 999999999999);
		deviceGpu.resetFences({*sc.headlessCopyDoneFences[fd.scIndx]});
		if (0) {
			uint8_t* dbuf = (uint8_t*) finalImage.mem.mapMemory(0, windowHeight*windowWidth*4, {});
			uint8_t* buf = (uint8_t*) malloc(windowWidth*windowHeight*3);
			for (int y=0; y<windowHeight; y++)
				for (int x=0; x<windowWidth; x++)
					for (int c=0; c<3; c++) {
						buf[y*windowWidth*3+x*3+c] = dbuf[y*windowWidth*4+x*4+c];
					}
			finalImage.mem.unmapMemory();

			auto fd_ = open("tst.bmp", O_RDWR | O_TRUNC | O_CREAT, S_IRWXU);
			write(fd_, buf, windowWidth*windowHeight*3);
			close(fd_);
			free(buf);
		}
		// memcpy(dbuf, renderState.mvp(), 16*4);

	}
};


int main() {

	GlobeApp app;
	app.windowWidth = 1700;
	// app.windowWidth = 1000;
	// app.windowWidth = 900;
	app.windowHeight = 800;
	// app.headless = true;
	app.headless = false;

	app.initVk();

	while (not app.isDone()) {
		//if (proc) app.render();
		app.render();
		//usleep(33'000);
	}

	return 0;
}
