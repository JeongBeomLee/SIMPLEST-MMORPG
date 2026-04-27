-- ============================================
-- SIMPLEST MMORPG — DB 스키마 + Stored Procedure
-- ============================================
-- 사용 방법:
--   1. 먼저 mmorpg_dev DB와 mmorpg_user 로그인을 만든다 (README 참고)
--   2. SSMS에서 mmorpg_dev DB를 선택한 후 이 스크립트를 실행
--
-- 모든 SP는 idempotent (CREATE OR ALTER) — 재실행 안전
-- ============================================

USE mmorpg_dev;
GO

-- ============================================
-- Players 테이블
-- ============================================
IF OBJECT_ID('dbo.Players', 'U') IS NULL
BEGIN
    CREATE TABLE dbo.Players
    (
        id          BIGINT       IDENTITY(1,1) PRIMARY KEY,
        name        NVARCHAR(32) NOT NULL UNIQUE,
        level       SMALLINT     NOT NULL DEFAULT 1,
        exp         INT          NOT NULL DEFAULT 0,
        hp          INT          NOT NULL DEFAULT 100,
        max_hp      INT          NOT NULL DEFAULT 100,
        x           SMALLINT     NOT NULL DEFAULT 0,
        y           SMALLINT     NOT NULL DEFAULT 0,
        created_at  DATETIME2    NOT NULL DEFAULT SYSUTCDATETIME(),
        last_login  DATETIME2    NULL
    );
END;
GO

-- ============================================
-- SP: 이름으로 플레이어 조회 (로그인 시)
-- ============================================
CREATE OR ALTER PROCEDURE dbo.sp_LoadPlayerByName
    @name NVARCHAR(32)
AS
BEGIN
    SET NOCOUNT ON;

    SELECT id, name, level, exp, hp, max_hp, x, y
    FROM dbo.Players
    WHERE name = @name;
END;
GO

-- ============================================
-- SP: 신규 플레이어 생성
--     - 기본값으로 INSERT 후 SCOPE_IDENTITY()로 새 id 반환
--     - name UNIQUE 제약 위반 시 ODBC 측에서 에러 진단
-- ============================================
CREATE OR ALTER PROCEDURE dbo.sp_CreatePlayer
    @name NVARCHAR(32)
AS
BEGIN
    SET NOCOUNT ON;

    INSERT INTO dbo.Players (name, last_login)
    VALUES (@name, SYSUTCDATETIME());

    SELECT SCOPE_IDENTITY() AS id;
END;
GO

-- ============================================
-- SP: 플레이어 상태 저장 (주기 저장 + 로그아웃 시)
-- ============================================
CREATE OR ALTER PROCEDURE dbo.sp_SavePlayer
    @id     BIGINT,
    @level  SMALLINT,
    @exp    INT,
    @hp     INT,
    @max_hp INT,
    @x      SMALLINT,
    @y      SMALLINT
AS
BEGIN
    SET NOCOUNT ON;

    UPDATE dbo.Players
    SET level  = @level,
        exp    = @exp,
        hp     = @hp,
        max_hp = @max_hp,
        x      = @x,
        y      = @y
    WHERE id = @id;
END;
GO

-- ============================================
-- SP: 마지막 로그인 시각 갱신 (로그인 직후)
-- ============================================
CREATE OR ALTER PROCEDURE dbo.sp_UpdateLastLogin
    @id BIGINT
AS
BEGIN
    SET NOCOUNT ON;

    UPDATE dbo.Players
    SET last_login = SYSUTCDATETIME()
    WHERE id = @id;
END;
GO

-- ============================================
-- 검증 쿼리 (수동 실행)
-- ============================================
-- SP 4개 다 만들어졌는지 확인:
--   SELECT name FROM sys.procedures WHERE name LIKE 'sp_%';
--
-- 테이블 존재 확인:
--   SELECT * FROM dbo.Players;
--
-- 동작 테스트:
--   EXEC dbo.sp_CreatePlayer @name = N'TestUser';
--   EXEC dbo.sp_LoadPlayerByName @name = N'TestUser';
--   EXEC dbo.sp_UpdateLastLogin @id = 1;
--   SELECT * FROM dbo.Players;
--
-- 정리 (StressTest 봇 일괄 삭제):
--   DELETE FROM dbo.Players WHERE name LIKE 'bot_%';
--
-- ID 카운터 리셋 (테이블 비운 후):
--   DBCC CHECKIDENT ('dbo.Players', RESEED, 0);

-- ============================================
-- 한국 시간(KST) 변환 뷰 (선택)
--   SYSUTCDATETIME()가 UTC라 한국시간으로 보고 싶을 때
-- ============================================
CREATE OR ALTER VIEW dbo.vw_PlayersKst
AS
SELECT
    id, name, level, exp, hp, max_hp, x, y,
    SWITCHOFFSET(CAST(created_at AS DATETIMEOFFSET), '+09:00') AS created_at_kst,
    SWITCHOFFSET(CAST(last_login AS DATETIMEOFFSET), '+09:00') AS last_login_kst
FROM dbo.Players;
GO

-- 사용:
--   SELECT * FROM dbo.vw_PlayersKst;
