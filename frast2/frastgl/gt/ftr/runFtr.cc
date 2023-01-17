#include <unistd.h>

#include "frastgl/core/app.h"
#include "frastgl/core/render_state.h"
#include "frastgl/extra/earth/earth.h"
#include "ftr.h"

#include <chrono>

using namespace frast;

//
// No need for this to not use the main thread -- except that is an example for future applications.
//

class TestApp : public App {
	public:

		inline TestApp(const AppConfig& cfg)
			: App(cfg)
		{
			thread = std::thread(&TestApp::loop, this);
		}

		inline ~TestApp() {
			assert(isDone and "you should not destroy TestApp until done");
			if (thread.joinable()) thread.join();
		}

		virtual void render(RenderState& rs) override {
			window.beginFrame();

			if (moveCaster) {
				Eigen::Map<const RowMatrix4d> view { rs.view() };
				Eigen::Map<const RowMatrix4d> proj { rs.proj() };
				RowMatrix4f matrix = (proj*view).cast<float>();
				ftr->cwd.setMatrix1(matrix.data());
			}

			// fmt::print(" - render\n");
			glClearColor(0,0,.01,1);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			ftr->defaultUpdate(rs.camera);
			ftr->render(rs);

			earthEllps->render(rs);


			window.endFrame();
			
		}


		virtual void doInit() override {
			FtTypes::Config cfg;
			cfg.debugMode = true;

			cfg.obbIndexPaths = {"/data/naip/mocoNaip/moco.fft.obb"};
			cfg.colorDsetPaths = {"/data/naip/mocoNaip/moco.fft"};

			ftr = std::make_unique<FtRenderer>(cfg);
			ftr->init(this->cfg);
			setExampleCasterData();

			earthEllps = std::make_unique<EarthEllipsoid>();


		}

		inline virtual bool handleKey(int key, int scancode, int action, int mods) override {
			if (action == GLFW_PRESS and key == GLFW_KEY_M) moveCaster = !moveCaster;
			return false;
		}

	protected:

		std::unique_ptr<FtRenderer> ftr;
		std::unique_ptr<EarthEllipsoid> earthEllps;

		std::thread thread;

		inline void loop() {
			int frames=0;

			init();

			CameraSpec spec(cfg.w, cfg.h, 45.0 * M_PI/180);
			SphericalEarthMovingCamera cam(spec);

			Eigen::Vector3d pos0 { 0.136273, -1.03348, 0.552593 };
			Eigen::Matrix<double,3,3,Eigen::RowMajor> R0;
			R0.row(2) = -pos0.normalized();
			R0.row(0) =  R0.row(2).cross(Eigen::Vector3d::UnitZ()).normalized();
			R0.row(1) =  R0.row(2).cross(R0.row(0)).normalized();
			R0.transposeInPlace();
			cam.setPosition(pos0.data());
			cam.setRotMatrix(R0.data());

			window.addIoUser(&cam);

			// FIXME: call glViewport...

			auto last_time = std::chrono::high_resolution_clock::now();

			while (true) {
				usleep(33'000);
				frames++;

				auto now_time = std::chrono::high_resolution_clock::now();
				float dt = std::chrono::duration_cast<std::chrono::microseconds>(now_time - last_time).count() * 1e-6;
				last_time = now_time;

				cam.step(dt);
				RenderState rs(&cam);
				rs.frameBegin();

				render(rs);

				if (frames>5000) break;
			}

			fmt::print(" - Destroying ftr in render thread\n");
			ftr = nullptr;

			isDone = true;
		}

		bool moveCaster = true;
		void setExampleCasterData() {
			// ftr->setCasterInRenderThread
			float color[4] = {.1f,0.f,.1f,.1f};
			// Image tstImg { 512,512,Image::Format::RGBA };
			cv::Mat tstImg(512,512,CV_8UC4);
			for (int y=0; y<512; y++)
			for (int x=0; x<512; x++) {
				uint8_t c = (y % 16 <= 4 and x % 16 <= 4) ? 200 : 0;
				static_cast<uint8_t*>(tstImg.data)[y*512*4+x*4+0] =
				static_cast<uint8_t*>(tstImg.data)[y*512*4+x*4+1] =
				static_cast<uint8_t*>(tstImg.data)[y*512*4+x*4+2] = c;
				static_cast<uint8_t*>(tstImg.data)[y*512*4+x*4+3] = 200;
			}
			ftr->cwd.setImage(tstImg);
			ftr->cwd.setColor1(color);
			ftr->cwd.setColor2(color);
			ftr->cwd.setMask(0b01);
		}

	public:
		bool isDone = false;
};


int main() {


	AppConfig appCfg;
	// appCfg.w = 1024;
	appCfg.w = 1920;
	appCfg.h = 1080;

	TestApp app(appCfg);

	while (!app.isDone) sleep(1);


	return 0;
}
