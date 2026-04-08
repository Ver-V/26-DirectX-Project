#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <vector>
#include <string>
#include <chrono>
#include <cstring>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

struct VideoConfig
{
    const int Width = 800;
    const int Height = 600;
    bool IsFullscreen = false;
    int VSync = 1;
} g_Config;

struct Vertex
{
    float x, y, z;
    float r, g, b, a;
};

HWND g_hWnd = nullptr;
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
ID3D11VertexShader* g_pVertexShader = nullptr;
ID3D11PixelShader* g_pPixelShader = nullptr;
ID3D11InputLayout* g_pInputLayout = nullptr;

const char* shaderSource = R"(
struct VS_IN { float3 pos : POSITION; float4 col : COLOR; };
struct PS_IN { float4 pos : SV_POSITION; float4 col : COLOR; };
PS_IN VS(VS_IN input) { PS_IN output; output.pos = float4(input.pos, 1.0f); output.col = input.col; return output; }
float4 PS(PS_IN input) : SV_Target { return input.col; }
)";

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_DESTROY)
    {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

class Component
{
public:
    class GameObject* pOwner = nullptr;
    bool isStarted = 0;

    virtual void Start() = 0;
    virtual void Input() {}
    virtual void Update(float dt) = 0;
    virtual void Render() {}
    virtual ~Component() {}
};

class GameObject
{
public:
    std::string name;
    float x, y;
    std::vector<Component*> components;

    GameObject(std::string n, float px, float py)
    {
        name = n;
        x = px;
        y = py;
    }

    ~GameObject()
    {
        for (int i = 0; i < (int)components.size(); i++)
        {
            delete components[i];
        }
    }

    void AddComponent(Component* pComp)
    {
        pComp->pOwner = this;
        pComp->isStarted = false;
        components.push_back(pComp);
    }
};

class PlayerControl : public Component
{
public:
    float speed;
    bool moveUp, moveDown, moveLeft, moveRight;
    int playerType;

    PlayerControl(int type)
    {
        playerType = type;
    }

    void Start() override
    {
        speed = 250.0f;
        moveUp = moveDown = moveLeft = moveRight = false;
    }

    void Input() override
    {
        if (playerType == 0)
        {
            moveUp = (GetAsyncKeyState('W') & 0x8000);
            moveDown = (GetAsyncKeyState('S') & 0x8000);
            moveLeft = (GetAsyncKeyState('A') & 0x8000);
            moveRight = (GetAsyncKeyState('D') & 0x8000);
        }
        if (playerType == 1)
        {
            moveUp = (GetAsyncKeyState(VK_UP) & 0x8000);
            moveDown = (GetAsyncKeyState(VK_DOWN) & 0x8000);
            moveLeft = (GetAsyncKeyState(VK_LEFT) & 0x8000);
            moveRight = (GetAsyncKeyState(VK_RIGHT) & 0x8000);
        }
    }

    void Update(float dt) override
    {
        if (moveUp)    pOwner->y -= speed * dt;
        if (moveDown)  pOwner->y += speed * dt;
        if (moveLeft)  pOwner->x -= speed * dt;
        if (moveRight) pOwner->x += speed * dt;

        if (pOwner->x < 40.0f) pOwner->x = 40.0f;
        if (pOwner->x > 760.0f) pOwner->x = 760.0f;
        if (pOwner->y < 40.0f) pOwner->y = 40.0f;
        if (pOwner->y > 560.0f) pOwner->y = 560.0f;
    }
};

class TriangleRenderer : public Component
{
public:
    float r, g, b, a;
    ID3D11Buffer* pVertexBuffer = nullptr;

    TriangleRenderer(float cr, float cg, float cb, float ca)
    {
        r = cr;
        g = cg;
        b = cb;
        a = ca;
    }

    ~TriangleRenderer()
    {
        if (pVertexBuffer) pVertexBuffer->Release();
    }

    void Start() override
    {
        Vertex vertices[] =
        {
            {  0.0f,  0.1f, 0.5f, r, g, b, a },
            {  0.1f, -0.1f, 0.5f, r, g, b, a },
            { -0.1f, -0.1f, 0.5f, r, g, b, a },
        };

        D3D11_BUFFER_DESC bd = { sizeof(vertices), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
        D3D11_SUBRESOURCE_DATA initData = { vertices, 0, 0 };
        g_pd3dDevice->CreateBuffer(&bd, &initData, &pVertexBuffer);
    }

    void Update(float dt) override
    {
    }

    void Render() override
    {
        float x = pOwner->x / 400.0f - 1.0f;
        float y = 1.0f - pOwner->y / 300.0f;

        Vertex vertices[3];

        if (pOwner->name == "Player2")
        {
            vertices[0] = { x,         y - 0.10f, 0.5f, r, g, b, a };
            vertices[1] = { x - 0.10f, y + 0.10f, 0.5f, r, g, b, a };
            vertices[2] = { x + 0.10f, y + 0.10f, 0.5f, r, g, b, a };
        }
        else
        {
            vertices[0] = { x,         y + 0.10f, 0.5f, r, g, b, a };
            vertices[1] = { x + 0.10f, y - 0.10f, 0.5f, r, g, b, a };
            vertices[2] = { x - 0.10f, y - 0.10f, 0.5f, r, g, b, a };
        }

        g_pImmediateContext->UpdateSubresource(pVertexBuffer, 0, nullptr, vertices, 0, 0);

        UINT stride = sizeof(Vertex);
        UINT offset = 0;
        g_pImmediateContext->IASetVertexBuffers(0, 1, &pVertexBuffer, &stride, &offset);
        g_pImmediateContext->Draw(3, 0);
    }
};

class GameLoop
{
public:
    bool isRunning;
    std::vector<GameObject*> gameWorld;
    std::chrono::high_resolution_clock::time_point prevTime;
    float deltaTime;

    void Initialize()
    {
        isRunning = true;
        gameWorld.clear();
        prevTime = std::chrono::high_resolution_clock::now();
        deltaTime = 0.0f;
    }

    void Input()
    {
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) isRunning = false;
        if (GetAsyncKeyState('F') & 0x0001)
        {
            g_Config.IsFullscreen = !g_Config.IsFullscreen;
            g_pSwapChain->SetFullscreenState(g_Config.IsFullscreen, nullptr);
        }

        for (int i = 0; i < (int)gameWorld.size(); i++)
        {
            for (int j = 0; j < (int)gameWorld[i]->components.size(); j++)
            {
                gameWorld[i]->components[j]->Input();
            }
        }
    }

    void Update()
    {
        for (int i = 0; i < (int)gameWorld.size(); i++)
        {
            for (int j = 0; j < (int)gameWorld[i]->components.size(); j++)
            {
                if (gameWorld[i]->components[j]->isStarted == false)
                {
                    gameWorld[i]->components[j]->Start();
                    gameWorld[i]->components[j]->isStarted = true;
                }
            }
        }

        for (int i = 0; i < (int)gameWorld.size(); i++)
        {
            for (int j = 0; j < (int)gameWorld[i]->components.size(); j++)
            {
                gameWorld[i]->components[j]->Update(deltaTime);
            }
        }
    }

    void Render()
    {
        float clearColor[] = { 0.1f, 0.2f, 0.3f, 1.0f };
        g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);

        D3D11_VIEWPORT vp;
        vp.TopLeftX = 0;
        vp.TopLeftY = 0;
        vp.Width = (float)g_Config.Width;
        vp.Height = (float)g_Config.Height;
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        g_pImmediateContext->RSSetViewports(1, &vp);

        g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);
        g_pImmediateContext->IASetInputLayout(g_pInputLayout);
        g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
        g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);

        for (int i = 0; i < (int)gameWorld.size(); i++)
        {
            for (int j = 0; j < (int)gameWorld[i]->components.size(); j++)
            {
                gameWorld[i]->components[j]->Render();
            }
        }

        g_pSwapChain->Present(g_Config.VSync, 0);
    }

    void Run()
    {
        MSG msg = { 0 };

        while (isRunning)
        {
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                if (msg.message == WM_QUIT) isRunning = false;
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            std::chrono::high_resolution_clock::time_point currentTime = std::chrono::high_resolution_clock::now();
            std::chrono::duration<float> elapsed = currentTime - prevTime;
            deltaTime = elapsed.count();
            prevTime = currentTime;

            Input();
            Update();
            Render();
        }
    }

    GameLoop()
    {
        Initialize();
    }

    ~GameLoop()
    {
        for (int i = 0; i < (int)gameWorld.size(); i++)
        {
            delete gameWorld[i];
        }
    }
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.lpszClassName = L"DX11GameClass";
    RegisterClassExW(&wcex);

    RECT rc = { 0, 0, g_Config.Width, g_Config.Height };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    g_hWnd = CreateWindowW(L"DX11GameClass", L"TriangleProgram",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hInstance, nullptr);
    ShowWindow(g_hWnd, nCmdShow);

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = g_Config.Width;
    sd.BufferDesc.Height = g_Config.Height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = g_hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext);

    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release();

    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    D3DCompile(shaderSource, std::strlen(shaderSource), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vsBlob, nullptr);
    D3DCompile(shaderSource, std::strlen(shaderSource), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &psBlob, nullptr);
    g_pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_pVertexShader);
    g_pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_pPixelShader);

    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    g_pd3dDevice->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &g_pInputLayout);
    vsBlob->Release();
    psBlob->Release();

    GameLoop gameLoop;

    GameObject* player1 = new GameObject("Player1", 200.0f, 300.0f);
    player1->AddComponent(new PlayerControl(0));
    player1->AddComponent(new TriangleRenderer(1.0f, 0.0f, 0.0f, 1.0f));
    gameLoop.gameWorld.push_back(player1);

    GameObject* player2 = new GameObject("Player2", 600.0f, 300.0f);
    player2->AddComponent(new PlayerControl(1));
    player2->AddComponent(new TriangleRenderer(1.0f, 1.0f, 0.0f, 1.0f));
    gameLoop.gameWorld.push_back(player2);

    gameLoop.Run();

    if (g_pSwapChain && g_Config.IsFullscreen) g_pSwapChain->SetFullscreenState(FALSE, nullptr);
    if (g_pInputLayout) g_pInputLayout->Release();
    if (g_pVertexShader) g_pVertexShader->Release();
    if (g_pPixelShader) g_pPixelShader->Release();
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    if (g_pSwapChain) g_pSwapChain->Release();
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pd3dDevice) g_pd3dDevice->Release();

    return 0;
}