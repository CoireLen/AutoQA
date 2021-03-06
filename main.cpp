#include "autogui.h"
#include "cnocr.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers
#include <thread>
#include "difflib.h"
#include <xlnt/xlnt.hpp>
#include <codecvt>
void ocrscreen(std::wstring &ws_q);
void testimg();
void selectrect(int *rect);
void on_mouse(int event, int x, int y, int flags, void *ustc);
cnocr *pocr;
autogui *pag;
static int startstate = 0;
int q_area[4] = {0}; //问题识别区域
static int recognition_interval = 500; //识别间隔
std::vector<std::vector<int>> va_area_i4;
// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to maximize ease of testing and compatibility with old VS compilers.
// To link with VS2010-era libraries, VS2015+ requires linking with legacy_stdio_definitions.lib, which we do using this pragma.
// Your own project should not be affected, as you are likely to link with a newer binary of GLFW that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

static void glfw_error_callback(int error, const char *description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

int main(int, char **)
{
    // Setup window
    cnocr ocr;
    pocr = &ocr;
    autogui ag;
    pag = &ag;
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

        // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char *glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char *glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);           // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char *glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    // glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

    // Create window with graphics context
    GLFWwindow *window = glfwCreateWindow(700, 500, "AutoQA", NULL, NULL);
    if (window == NULL)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsClassic();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyh.ttc", 24.0f, NULL, io.Fonts->GetGlyphRangesChineseFull());

    // Our state
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    std::vector<std::thread> vthread;
    static int a_area[4] = {0};         //答案识别区域
    static char *a_area_data[20] = {0}; //答案识别区域文字,用于显示已添加的答案识别区域
    static int a_area_count = 0;        //识别区域计数
    std::wstring ws_question;
    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
        {

            static float f = 0.0f;
            static int counter = 0;
            static char *startbutton[] = {u8"开始自动答题", u8"正在运行...",u8"正在停止....."};
            static char loadsave_filename[100]="def.txt";
            static int loadsaveerr = 0;
            static char *loadsaveerr_data[]={u8"",u8"文件无法打开请检查文件名",u8""} ;
            static int a_area_select = 0;          //答案识别区域当前选中
            static ImGuiWindowFlags wflag = ImGuiWindowFlags_NoCollapse;
            static unsigned char *imgdata = NULL;

            ImGui::Begin(u8"自动答题工具", (bool *)0, wflag);
            //保存或载入配置
            ImGui::BeginGroup();
            ImGui::InputText(u8"保存或载入文件",loadsave_filename,sizeof(loadsave_filename));

            if (ImGui::Button(u8"载入配置"))
            {
                std::fstream fp;
                fp.open(std::string(loadsave_filename),std::ios::in);
                if (fp.is_open()){
                    char buf[200];
                    fp.getline(buf,199);
                    sscanf(buf,"%4d %4d %4d %4d",&q_area[0],&q_area[1],&q_area[2],&q_area[3]);
                    while(fp.getline(buf,199)){
                        sscanf(buf,"%4d %4d %4d %4d",&a_area[0],&a_area[1],&a_area[2],&a_area[3]);
                        a_area_data[a_area_count] = new char[20];
                        sprintf(a_area_data[a_area_count++], "%4d %4d %4d %4d", a_area[0], a_area[1], a_area[2], a_area[3]);
                        va_area_i4.push_back(std::vector<int>{a_area[0], a_area[1], a_area[2], a_area[3]});
                    }
                    fp.close();
                }
                else{
                    loadsaveerr=1;
                }
                
            }
            if (ImGui::Button(u8"保存配置"))
            {
                std::fstream fp;
                fp.open(std::string(loadsave_filename),std::ios::out);
                if (fp.is_open()){
                    char buf[200];
                    sprintf(buf,"%4d %4d %4d %4d",q_area[0],q_area[1],q_area[2],q_area[3]);
                    fp<<buf<<std::endl;
                    for (int i=0;i<a_area_count;i++){
                        fp<<a_area_data[i]<<std::endl;
                    }
                }
            }
            ImGui::EndGroup();
            //添加问题识别区域
            ImGui::BeginGroup();
            ImGui::InputInt4(u8"问题区域左上右下", q_area);
            if (ws_question.size()!=0){
                ImGui::Text(u8"识别为:%ls",ws_question.c_str());
            }
            else{
                ImGui::Text(u8"识别为:[空]");
            }
            if (ImGui::Button(u8"选择问题识别区域"))
            {
                vthread.push_back(std::thread(selectrect,q_area));
            }
            ImGui::EndGroup();
            //添加答案识别区域
            ImGui::BeginGroup();
            ImGui::ListBox(u8"已添加的答案识别区域", &a_area_select, a_area_data, a_area_count);
            ImGui::InputInt4(u8"答案区域左上右下", a_area);
            if (ImGui::Button(u8"选择答案识别区域")){
                vthread.push_back(std::thread(selectrect,a_area));
            }
            ImGui::SameLine();
            if (ImGui::Button(u8"添加答案识别区域"))
            {
                if (a_area_count < 20)
                {
                    a_area_data[a_area_count] = new char[20];
                    sprintf(a_area_data[a_area_count++], "%4d %4d %4d %4d", a_area[0], a_area[1], a_area[2], a_area[3]);
                    va_area_i4.push_back(std::vector<int>{a_area[0], a_area[1], a_area[2], a_area[3]});
                }
            }

            ImGui::EndGroup();
            //识别间隔
            ImGui::SliderInt(u8"识别间隔(ms)", &recognition_interval, 0, 2000);

            if (ImGui::Button(startbutton[startstate]))
            {
                //这里分为3个状态
                // 0:未启动
                // 1:启动
                // 2:启动但未停止
                switch (startstate)
                {
                case 0:
                    startstate = 1;
                    vthread.push_back(std::thread(ocrscreen,std::ref(ws_question)));
                    break;
                case 1:
                    startstate = 2;
                    break;
                default:
                    break;
                }
            }
            ImGui::End();
        }
        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }
    for (auto i = 0; i < vthread.size(); i++)
    {
        vthread[i].join();
    }
    for (auto i = 0; i < a_area_count; i++)
    {
        delete[] a_area_data[i];
    }
    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
std::wstring utf8ToWstring(const std::string &str)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> strCnv;
    return strCnv.from_bytes(str);
}
struct cvmousedrawrectdata
{
    cv::Mat *srcimg;
    cv::Mat img;
    cv::Mat temp1;
    bool  drawrect=false;
    cv::Point leftpoint=cv::Point(-1,-1);
    cv::Point mousepoint=cv::Point(-1,-1);
    int *rect;
};

void selectrect(int * rect){
    auto img=pag->screen.capture();
    cv::Mat matimg(pag->ScreenSize.y,pag->ScreenSize.x,CV_8UC4,img);
    cv::namedWindow("select rect",cv::WINDOW_NORMAL);
    cv::setWindowProperty("select rect",cv::WND_PROP_FULLSCREEN,cv::WINDOW_FULLSCREEN );//CV_WND_PROP_FULLSCREEN，CV_WINDOW_FULLSCREEN
    struct cvmousedrawrectdata mddate;
    mddate.srcimg=&matimg;
    mddate.img=matimg.clone();
    mddate.temp1=matimg.clone();
    mddate.rect=rect;
    while (cv::waitKey(30) != 27){
        cv::setMouseCallback("select rect", on_mouse, (void*)&mddate);
        putText(mddate.temp1,"("+std::to_string(mddate.mousepoint.x)+","+std::to_string(mddate.mousepoint.y)+")" , mddate.mousepoint, cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255,0, 0,255));
        cv::imshow("select rect",mddate.temp1);
    }
    cv::destroyWindow("select rect");
    delete img;
}
void on_mouse(int event, int x, int y, int flags, void *ustc){
    struct cvmousedrawrectdata* mddate=(struct cvmousedrawrectdata*)ustc;
    cv::Mat& image = mddate->img;//这样就可以传递Mat信息了
    cv::Mat& temp1=mddate->temp1;
    image.copyTo(temp1);
	char temp[16];
	switch (event) {
	case cv::EVENT_LBUTTONDOWN://按下左键
	{   
        mddate->srcimg->copyTo(image);
		sprintf(temp, "(%d,%d)", x, y);
		putText(image, temp, cv::Point(x, y), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 0, 0, 255));
		mddate->drawrect = true;
		mddate->leftpoint= cv::Point(x, y);
        mddate->rect[0]=x;
        mddate->rect[1]=y;
	}	break;
	case cv::EVENT_MOUSEMOVE://移动鼠标
	{
		mddate->mousepoint = cv::Point(x, y);
		if (mddate->drawrect)
		{ 
            cv::rectangle(temp1, mddate->leftpoint, mddate->mousepoint, cv::Scalar(0,255,0,255));
        }
	}break;
	case cv::EVENT_LBUTTONUP:
	{
		mddate->drawrect = false;
		sprintf(temp, "(%d,%d)", x, y);
		putText(image, temp, cv::Point(x, y), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 0, 0,255));
		//调用函数进行绘制
		cv::rectangle(image,mddate->leftpoint, mddate->mousepoint, cv::Scalar(0,255,0,255));
        mddate->rect[2]=x;
        mddate->rect[3]=y;
	}break;
    }
}
void ocrscreen(std::wstring &ws_question)
{
    xlnt::workbook wb;
    wb.load("QA.xlsx");
    auto ws = wb.sheet_by_index(0);
    auto xlsxrows = ws.rows(false);

    while (startstate == 1)
    {
        auto imgdata = pag->screen.capture();
        cv::Mat img(pag->ScreenSize.y, pag->ScreenSize.x, CV_8UC4, imgdata);
        auto imgrect = img(cv::Rect(q_area[0], q_area[1], q_area[2] - q_area[0], q_area[3] - q_area[1]));
        auto qout = pocr->ocr(imgrect);
        ws_question.clear();
        std::wstring ws_q;
        for (auto i : qout)
        {
            ws_q.append(i);
        }
        ws_question.append(ws_q);
        for (auto i = 0; i < xlsxrows.length(); i++)
        {
            auto mq = difflib::SequenceMatcher<std::wstring>(utf8ToWstring(xlsxrows[i][0].to_string()), ws_q);
            if (mq.ratio() > 0.7)
            {
                auto ans=utf8ToWstring(xlsxrows[i][1].to_string());//答案
                for (auto area:va_area_i4){
                    imgrect = img(cv::Rect(area[0], area[1], area[2] - area[0], area[3] - area[1]));
                    auto aout = pocr->ocr(imgrect);
                    std::wstring ws_a;//
                    for (auto line:aout){
                        ws_a.append(line);
                    }
                    std::wcout<< ws_a<<std::endl;
                    auto ma = difflib::SequenceMatcher<std::wstring>(ans, ws_a);
                    if (ma.ratio()>0.7){
                        pag->click(area[0]+(area[2] - area[0])/2, area[1]+(area[3] - area[1])/2);
                    }
                }
            }
        }
        delete imgdata;
        if (startstate == 1){
            Sleep(recognition_interval);
        }
    }
    startstate = 0;
}