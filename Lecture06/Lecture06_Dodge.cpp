#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <cstring>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;
const float PI = 3.141592f;

struct Vertex
{
    float x, y, z;
    float r, g, b, a;
};

struct Rect
{
    float x, y, w, h;
};

struct Bullet
{
    float x, y, radius;
    float vx, vy;
};

struct InputState
{
    bool left;
    bool right;
    bool up;
    bool down;
};

float Clamp(float value, float minValue, float maxValue)
{
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

class DodgeGame
{
public:
    DodgeGame()
    {
        m_hWnd = nullptr;
        m_device = nullptr;
        m_context = nullptr;
        m_swapChain = nullptr;
        m_renderTargetView = nullptr;
        m_vertexBuffer = nullptr;
        m_vertexShader = nullptr;
        m_pixelShader = nullptr;
        m_inputLayout = nullptr;
        m_score = 0.0f;

        m_input.left = false;
        m_input.right = false;
        m_input.up = false;
        m_input.down = false;
    }

    ~DodgeGame()
    {
        Cleanup();
    }

    bool Initialize(HWND hWnd)
    {
        m_hWnd = hWnd;

        if (!InitD3D())
        {
            return false;
        }

        InitGame();
        return true;
    }

    void Input()
    {
        m_input.left = false;
        m_input.right = false;
        m_input.up = false;
        m_input.down = false;

        if (GetAsyncKeyState(VK_LEFT) & 0x8000 || GetAsyncKeyState('A') & 0x8000)
        {
            m_input.left = true;
        }

        if (GetAsyncKeyState(VK_RIGHT) & 0x8000 || GetAsyncKeyState('D') & 0x8000)
        {
            m_input.right = true;
        }

        if (GetAsyncKeyState(VK_UP) & 0x8000 || GetAsyncKeyState('W') & 0x8000)
        {
            m_input.up = true;
        }

        if (GetAsyncKeyState(VK_DOWN) & 0x8000 || GetAsyncKeyState('S') & 0x8000)
        {
            m_input.down = true;
        }
    }

    void Update(float deltaTime)
    {
        m_score += 10.0f * deltaTime;

        float playerSpeed = 260.0f;

        if (m_input.left) m_player.x -= playerSpeed * deltaTime;
        if (m_input.right) m_player.x += playerSpeed * deltaTime;
        if (m_input.up) m_player.y -= playerSpeed * deltaTime;
        if (m_input.down) m_player.y += playerSpeed * deltaTime;

        m_player.x = Clamp(m_player.x, 0.0f, (float)SCREEN_WIDTH - m_player.w);
        m_player.y = Clamp(m_player.y, 0.0f, (float)SCREEN_HEIGHT - m_player.h);

        for (int i = 0; i < (int)m_bullets.size(); ++i)
        {
            m_bullets[i].x += m_bullets[i].vx * deltaTime;
            m_bullets[i].y += m_bullets[i].vy * deltaTime;

            if (m_bullets[i].x - m_bullets[i].radius < 0.0f)
            {
                m_bullets[i].x = m_bullets[i].radius;
                m_bullets[i].vx *= -1.0f;
            }

            if (m_bullets[i].x + m_bullets[i].radius > SCREEN_WIDTH)
            {
                m_bullets[i].x = SCREEN_WIDTH - m_bullets[i].radius;
                m_bullets[i].vx *= -1.0f;
            }

            if (m_bullets[i].y - m_bullets[i].radius < 0.0f)
            {
                m_bullets[i].y = m_bullets[i].radius;
                m_bullets[i].vy *= -1.0f;
            }

            if (m_bullets[i].y + m_bullets[i].radius > SCREEN_HEIGHT)
            {
                m_bullets[i].y = SCREEN_HEIGHT - m_bullets[i].radius;
                m_bullets[i].vy *= -1.0f;
            }

            if (CheckCollision(m_bullets[i], m_player))
            {
                InitGame();
                return;
            }
        }

        wchar_t title[128];
        swprintf_s(title, L"DirectX 11 Dodge - Score: %d", (int)m_score);
        SetWindowText(m_hWnd, title);
    }

    void Render()
    {
        float clearColor[4] = { 0.05f, 0.05f, 0.08f, 1.0f };

        m_context->ClearRenderTargetView(m_renderTargetView, clearColor);

        std::vector<Vertex> vertices;

        AddRectangle(vertices, m_player, 0.1f, 0.8f, 1.0f);

        for (int i = 0; i < (int)m_bullets.size(); ++i)
        {
            AddCircle(vertices, m_bullets[i], 1.0f, 0.2f, 0.2f);
        }

        D3D11_MAPPED_SUBRESOURCE mappedResource;
        ZeroMemory(&mappedResource, sizeof(D3D11_MAPPED_SUBRESOURCE));

        HRESULT hr = m_context->Map(m_vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);

        if (FAILED(hr))
        {
            return;
        }

        memcpy(mappedResource.pData, vertices.data(), sizeof(Vertex) * vertices.size());
        m_context->Unmap(m_vertexBuffer, 0);

        UINT stride = sizeof(Vertex);
        UINT offset = 0;

        m_context->IASetInputLayout(m_inputLayout);
        m_context->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        m_context->VSSetShader(m_vertexShader, nullptr, 0);
        m_context->PSSetShader(m_pixelShader, nullptr, 0);

        m_context->Draw((UINT)vertices.size(), 0);

        m_swapChain->Present(1, 0);
    }

private:
    HWND m_hWnd;

    ID3D11Device* m_device;
    ID3D11DeviceContext* m_context;
    IDXGISwapChain* m_swapChain;
    ID3D11RenderTargetView* m_renderTargetView;
    ID3D11Buffer* m_vertexBuffer;
    ID3D11VertexShader* m_vertexShader;
    ID3D11PixelShader* m_pixelShader;
    ID3D11InputLayout* m_inputLayout;

    Rect m_player;
    std::vector<Bullet> m_bullets;
    InputState m_input;
    float m_score;

    float ToNdcX(float x)
    {
        return (x / SCREEN_WIDTH) * 2.0f - 1.0f;
    }

    float ToNdcY(float y)
    {
        return 1.0f - (y / SCREEN_HEIGHT) * 2.0f;
    }

    bool InitD3D()
    {
        DXGI_SWAP_CHAIN_DESC scd;
        ZeroMemory(&scd, sizeof(DXGI_SWAP_CHAIN_DESC));

        scd.BufferCount = 1;
        scd.BufferDesc.Width = SCREEN_WIDTH;
        scd.BufferDesc.Height = SCREEN_HEIGHT;
        scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scd.OutputWindow = m_hWnd;
        scd.SampleDesc.Count = 1;
        scd.Windowed = TRUE;

        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            0,
            nullptr,
            0,
            D3D11_SDK_VERSION,
            &scd,
            &m_swapChain,
            &m_device,
            nullptr,
            &m_context
        );

        if (FAILED(hr)) return false;

        ID3D11Texture2D* backBuffer = nullptr;

        hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);

        if (FAILED(hr)) return false;

        hr = m_device->CreateRenderTargetView(backBuffer, nullptr, &m_renderTargetView);
        backBuffer->Release();

        if (FAILED(hr)) return false;

        m_context->OMSetRenderTargets(1, &m_renderTargetView, nullptr);

        D3D11_VIEWPORT viewport;
        ZeroMemory(&viewport, sizeof(D3D11_VIEWPORT));

        viewport.TopLeftX = 0.0f;
        viewport.TopLeftY = 0.0f;
        viewport.Width = (float)SCREEN_WIDTH;
        viewport.Height = (float)SCREEN_HEIGHT;
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;

        m_context->RSSetViewports(1, &viewport);

        return InitShaderAndBuffer();
    }

    bool InitShaderAndBuffer()
    {
        const char* shaderCode =
            "struct VS_INPUT"
            "{"
            "    float3 pos : POSITION;"
            "    float4 color : COLOR;"
            "};"
            "struct PS_INPUT"
            "{"
            "    float4 pos : SV_POSITION;"
            "    float4 color : COLOR;"
            "};"
            "PS_INPUT VS(VS_INPUT input)"
            "{"
            "    PS_INPUT output;"
            "    output.pos = float4(input.pos, 1.0f);"
            "    output.color = input.color;"
            "    return output;"
            "}"
            "float4 PS(PS_INPUT input) : SV_TARGET"
            "{"
            "    return input.color;"
            "}";

        ID3DBlob* vsBlob = nullptr;
        ID3DBlob* psBlob = nullptr;
        ID3DBlob* errorBlob = nullptr;

        HRESULT hr = D3DCompile(shaderCode, strlen(shaderCode), nullptr, nullptr, nullptr, "VS", "vs_5_0", 0, 0, &vsBlob, &errorBlob);

        if (FAILED(hr)) return false;

        hr = D3DCompile(shaderCode, strlen(shaderCode), nullptr, nullptr, nullptr, "PS", "ps_5_0", 0, 0, &psBlob, &errorBlob);

        if (FAILED(hr))
        {
            vsBlob->Release();
            return false;
        }

        hr = m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vertexShader);

        if (FAILED(hr))
        {
            vsBlob->Release();
            psBlob->Release();
            return false;
        }

        hr = m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pixelShader);

        if (FAILED(hr))
        {
            vsBlob->Release();
            psBlob->Release();
            return false;
        }

        D3D11_INPUT_ELEMENT_DESC layout[2];

        layout[0].SemanticName = "POSITION";
        layout[0].SemanticIndex = 0;
        layout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
        layout[0].InputSlot = 0;
        layout[0].AlignedByteOffset = 0;
        layout[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
        layout[0].InstanceDataStepRate = 0;

        layout[1].SemanticName = "COLOR";
        layout[1].SemanticIndex = 0;
        layout[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        layout[1].InputSlot = 0;
        layout[1].AlignedByteOffset = 12;
        layout[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
        layout[1].InstanceDataStepRate = 0;

        hr = m_device->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_inputLayout);

        vsBlob->Release();
        psBlob->Release();

        if (FAILED(hr)) return false;

        D3D11_BUFFER_DESC bufferDesc;
        ZeroMemory(&bufferDesc, sizeof(D3D11_BUFFER_DESC));

        bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        bufferDesc.ByteWidth = sizeof(Vertex) * 8192;
        bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        hr = m_device->CreateBuffer(&bufferDesc, nullptr, &m_vertexBuffer);

        if (FAILED(hr)) return false;

        return true;
    }

    void InitGame()
    {
        std::srand((unsigned int)std::time(nullptr));

        m_score = 0.0f;

        m_player.x = 385.0f;
        m_player.y = 285.0f;
        m_player.w = 30.0f;
        m_player.h = 30.0f;

        m_bullets.clear();

        for (int i = 0; i < 10; ++i)
        {
            Bullet bullet;

            bullet.radius = 10.0f;
            bullet.x = (float)(std::rand() % (SCREEN_WIDTH - 20) + 10);
            bullet.y = (float)(std::rand() % (SCREEN_HEIGHT - 20) + 10);

            if (bullet.x > 300.0f && bullet.x < 500.0f && bullet.y > 200.0f && bullet.y < 400.0f)
            {
                bullet.x = 50.0f;
                bullet.y = 50.0f;
            }

            bullet.vx = (float)((std::rand() % 2 == 0 ? 1 : -1) * (120 + std::rand() % 160));
            bullet.vy = (float)((std::rand() % 2 == 0 ? 1 : -1) * (120 + std::rand() % 160));

            m_bullets.push_back(bullet);
        }
    }

    bool CheckCollision(const Bullet& bullet, const Rect& rect)
    {
        float closestX = Clamp(bullet.x, rect.x, rect.x + rect.w);
        float closestY = Clamp(bullet.y, rect.y, rect.y + rect.h);

        float dx = bullet.x - closestX;
        float dy = bullet.y - closestY;

        return dx * dx + dy * dy <= bullet.radius * bullet.radius;
    }

    void AddRectangle(std::vector<Vertex>& vertices, const Rect& rect, float r, float g, float b)
    {
        float left = ToNdcX(rect.x);
        float right = ToNdcX(rect.x + rect.w);
        float top = ToNdcY(rect.y);
        float bottom = ToNdcY(rect.y + rect.h);

        vertices.push_back({ left, top, 0.0f, r, g, b, 1.0f });
        vertices.push_back({ right, top, 0.0f, r, g, b, 1.0f });
        vertices.push_back({ left, bottom, 0.0f, r, g, b, 1.0f });

        vertices.push_back({ left, bottom, 0.0f, r, g, b, 1.0f });
        vertices.push_back({ right, top, 0.0f, r, g, b, 1.0f });
        vertices.push_back({ right, bottom, 0.0f, r, g, b, 1.0f });
    }

    void AddCircle(std::vector<Vertex>& vertices, const Bullet& bullet, float r, float g, float b)
    {
        const int segmentCount = 32;

        float centerX = ToNdcX(bullet.x);
        float centerY = ToNdcY(bullet.y);

        for (int i = 0; i < segmentCount; ++i)
        {
            float angle1 = 2.0f * PI * i / segmentCount;
            float angle2 = 2.0f * PI * (i + 1) / segmentCount;

            float x1 = bullet.x + cosf(angle1) * bullet.radius;
            float y1 = bullet.y + sinf(angle1) * bullet.radius;

            float x2 = bullet.x + cosf(angle2) * bullet.radius;
            float y2 = bullet.y + sinf(angle2) * bullet.radius;

            vertices.push_back({ centerX, centerY, 0.0f, r, g, b, 1.0f });
            vertices.push_back({ ToNdcX(x1), ToNdcY(y1), 0.0f, r, g, b, 1.0f });
            vertices.push_back({ ToNdcX(x2), ToNdcY(y2), 0.0f, r, g, b, 1.0f });
        }
    }

    void Cleanup()
    {
        if (m_vertexBuffer) m_vertexBuffer->Release();
        if (m_inputLayout) m_inputLayout->Release();
        if (m_vertexShader) m_vertexShader->Release();
        if (m_pixelShader) m_pixelShader->Release();
        if (m_renderTargetView) m_renderTargetView->Release();
        if (m_swapChain) m_swapChain->Release();
        if (m_context) m_context->Release();
        if (m_device) m_device->Release();
    }
};

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_DESTROY)
    {
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

bool InitWindow(HINSTANCE hInstance, int nCmdShow, HWND& hWnd)
{
    WNDCLASSEX wc;
    ZeroMemory(&wc, sizeof(WNDCLASSEX));

    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"DodgeWindowClass";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    if (!RegisterClassEx(&wc))
    {
        return false;
    }

    RECT wr = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    hWnd = CreateWindowEx(
        0,
        L"DodgeWindowClass",
        L"DirectX 11 Dodge",
        WS_OVERLAPPEDWINDOW,
        100,
        100,
        wr.right - wr.left,
        wr.bottom - wr.top,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!hWnd)
    {
        return false;
    }

    ShowWindow(hWnd, nCmdShow);
    return true;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    HWND hWnd = nullptr;

    if (!InitWindow(hInstance, nCmdShow, hWnd))
    {
        return 0;
    }

    DodgeGame game;

    if (!game.Initialize(hWnd))
    {
        return 0;
    }

    MSG msg;
    ZeroMemory(&msg, sizeof(MSG));

    LARGE_INTEGER frequency;
    LARGE_INTEGER prevTime;
    LARGE_INTEGER currentTime;

    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&prevTime);

    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            QueryPerformanceCounter(&currentTime);

            float deltaTime = (float)(currentTime.QuadPart - prevTime.QuadPart) / (float)frequency.QuadPart;
            prevTime = currentTime;

            game.Input();
            game.Update(deltaTime);
            game.Render();
        }
    }

    return (int)msg.wParam;
}